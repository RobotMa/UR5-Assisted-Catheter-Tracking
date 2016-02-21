#include "ros/ros.h"
#include "std_msgs/String.h"
#include <iostream>
#include <geometry_msgs/Point.h>
#include <tf/transform_broadcaster.h>
#include <math.h>
#include <active_echo_serial/Num.h> // Active Echo Mmessage Header File
#include <dynamic_reconfigure/server.h> // Dynamic Reconfigure Header File
#include <dynamic_reconfig/active_echo_localizationConfig.h> // Active Echo Localization Dynamic Reconfigure Header File

// Some unknown runtime error exists between this node and the ur5.launch in ur5_control

//This node subscribes to the ROS topic /active_echo_data and publishes
//the coordinate information of the segmented point.

static int step = 1;              // Step length along x-axis
static int step_scaler = 1;
static int count = 0;
static bool g_localize = false;

void dynamiconfigCallback( dynamic_reconfig::active_echo_localizationConfig &config, uint32_t level)
{
	g_localize = config.Enable_Localization;
	step = config.Step_Length;
	step_scaler = config.Step_Scaler; 
	ROS_INFO("Reconfigure Request: %s %i %i", 
		  config.Enable_Localization?"True":"False",
		  config.Step_Length,
		  config.Step_Scaler);
}


void localizeCallback( const active_echo_serial::Num::ConstPtr& msg)
{
	static tf::TransformBroadcaster br;

	// Transform to be broadcasted to ultrasound_sensor 
	tf::Transform transform;
	tf::Quaternion q;

	double element_w = 0.3; // mm small probe : 3 mm & large probe 4.5 mm
	double AE_SRate = 80*pow(10, 6); // hz
	double SOS = 1480; // m/s 
			   // This will vary depending on the medium (water/phantom)
	bool broadcast = true;

	// Linear ultrasound probe
	// Note: x and y are flipped so that the reference frame of the probe
	// and the robot based will be parallel to each while at working status

	// Initialize x, y, and z
	double x = 0.0; // Unit:m
	double y = 0.0; // Unit:m
	double z = 0.0; // Unit:m

	// Detection range along x-axis
	const double detect_range = 0.02; // Unit:m

	// Store previous and current cntNum (counter number)
	static int cntNum_pre = 0;
	static int cntNum_cur = 0;
	
	// Step size along x axis
	double delta_x = 0.0; // Unit:m

	const int t_s = 25; // counter threshold
	const double changeLowerBound = 0.05; // %
	const double changeUpperBound = 0.3;  // % 

	// Calculated the estimated position of AE element in the US frame
	try {

		y = ceil(msg->l_ta - 64.5)*element_w/1000; // Unit:m
		z = -(msg->dly)*(1/AE_SRate)*SOS; // Unit:m
		
		cntNum_cur = msg->tc;
		cntNum_pre = cntNum_cur;

		if (cntNum_pre != 0) { delta_x = ( cntNum_cur - cntNum_pre )/ cntNum_pre; }

		if ( fabs(delta_x) < changeLowerBound ) {

			x = 0.0; // Assume that the segmented point falls within the mid-plane 
		}
		else if ( fabs(delta_x) > changeUpperBound ) {
			
			x = 0.0; // Stop the robot when change on cntNum is too large (noisy data)

		}
		else {
			x = delta_x*detect_range*step;
		}

		// if ( isnan(x) ) {std::cout << "Prepare to throw" << std::endl; throw false;}

	}
	catch (bool fail) { 
		std::cout << "Rostopic /active_echo_data is not published, or tc is nonpositive." << std::endl;
		broadcast = false;
	}


	// Filter out outliers of /segment_point
	if ( abs(msg->dly) < 2600 && msg->tc > 0  && broadcast == true ) {

		transform.setOrigin( tf::Vector3(x, y, z));
		std::cout << "Value of x is " << x << std::endl;
		std::cout << "Value of y is " << y << std::endl;
		q.setRPY(0, 0, 0);
		transform.setRotation(q);
		std::cout << "Preparing to broadcast the transform" << std::endl;
		br.sendTransform(tf::StampedTransform(transform, ros::Time::now(),"ultrasound_sensor", "segment_point"));

	}
	else if ( abs(msg->dly) > 3000 && msg->tc > 30 && broadcast == true ) {
		transform.setOrigin (tf::Vector3(0.001,y,z));
		q.setRPY(0,0,0);
		transform.setRotation(q);
		br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "ultrasound_sensor", "segment_point"));
	}
	else {std::cout << "Broadcast to /segment_point failed "; }

	// Test whehter dynamic reconfigure changes the value of mid_plane
	std::cout << "Enable localization" << std::boolalpha << g_localize  << std::endl;

	// Use count to illustrate the callback frequency of "localization node"
	std::cout << count << std::endl;
	count++;
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "active_echo_localization");
	ros::NodeHandle n;

	int rate = 10*step_scaler;
	ros::Rate r(rate); // Hz

	ros::Subscriber sub = n.subscribe("active_echo_data", 5, localizeCallback);

	dynamic_reconfigure::Server<dynamic_reconfig::active_echo_localizationConfig> server;
	dynamic_reconfigure::Server<dynamic_reconfig::active_echo_localizationConfig>::CallbackType f;

	f = boost::bind(&dynamiconfigCallback, _1, _2);
	server.setCallback(f);

	 while (ros::ok()){
		ros::spinOnce();

		r.sleep();
	} 

	return 0;
}
