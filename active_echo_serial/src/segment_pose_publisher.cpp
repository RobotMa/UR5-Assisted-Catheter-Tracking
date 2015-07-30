#include "ros/ros.h"
#include "std_msgs/String.h"

#include <tf/transform_listener.h>
#include <tf/transform_datatypes.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/Point.h>
#include <math.h>

// This node uses the pose of /segment_point to calculate the new desired pose
// of /ee_link 

int main(int argc, char **argv)
{
	ros::init(argc, argv, "segment_pose_publisher");
	ros::NodeHandle n;

	ros::Publisher pub_pose = n.advertise<geometry_msgs::Pose>( "setpose", 1);

	tf::TransformListener listener_1;
	tf::TransformListener listener_2; // Add a 2nd transform listener trying to fix messed up tf

	ros::Rate r(10); // 10 Hz 

	while (n.ok()){


		tf::Transform transform_be;

		try{
        	// Calculate the new pose of /ee_link based on /segment_point
        	// Look up the transformation between /ee_link and /base_link
			std::string ref_frame( "base_link" );
			std::string tge_frame( "ee_link" );
			std::string tgs_frame( "segment_point" );
			std::string tgu_frame( "ultrasound_sensor" );


			tf::StampedTransform transform_ue;
			tf::StampedTransform transform_bs;
			listener_1.waitForTransform( tge_frame, tgu_frame, ros::Time(0), ros::Duration(0.8) );
			listener_1.lookupTransform( tgu_frame, tge_frame, ros::Time(0), transform_ue );
			listener_2.waitForTransform( ref_frame, tgs_frame, ros::Time(0), ros::Duration(0.8)  );
			listener_2.lookupTransform( ref_frame, tgs_frame, ros::Time(0), transform_bs );


			// debug nan x, y, z of des_pose
			std::cout << transform_bs.getOrigin().x() << std::endl;
			std::cout << transform_bs.getOrigin().y() << std::endl;	

			transform_be.mult(transform_bs, transform_ue);

			geometry_msgs::Pose des_pose;

			des_pose.position.x =  transform_be.getOrigin().x();
			des_pose.position.y =  transform_be.getOrigin().y();
			des_pose.position.z =  transform_be.getOrigin().z();

			des_pose.orientation.x = transform_be.getRotation().x();
			des_pose.orientation.y = transform_be.getRotation().y();
			des_pose.orientation.z = transform_be.getRotation().z();
			des_pose.orientation.w = transform_be.getRotation().w();

			pub_pose.publish( des_pose );

		}
		catch(tf::TransformException ex)
		{ std::cout << ex.what() << std::endl; }

		// tf is messed up 	

		/*
		geometry_msgs::Pose des_pose;

		des_pose.position.x =  transform_be.getOrigin().x();
		des_pose.position.y =  transform_be.getOrigin().y();
		des_pose.position.z =  transform_be.getOrigin().z();

		des_pose.orientation.x = transform_be.getRotation().x();
		des_pose.orientation.y = transform_be.getRotation().y();
		des_pose.orientation.z = transform_be.getRotation().z();
		des_pose.orientation.w = transform_be.getRotation().w();
		

		pub_pose.publish( des_pose );	
		*/
		ros::spin();

		r.sleep();

	}


	return 0;
}
