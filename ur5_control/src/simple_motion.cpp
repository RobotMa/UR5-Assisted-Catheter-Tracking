#include <ros/ros.h>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <unsupported/Eigen/MatrixFunctions>

#include <vector>
#include <algorithm>

#include <tf/transform_listener.h>
#include <tf/transform_datatypes.h>

#include <tf_conversions/tf_eigen.h>

#include <rviz_plot/lab1.h>
#include <rviz_animate/lab2.h>
#include <ur5_class/lab3.h>
#include <inverse_ur5/lab4.h>
#include <std_msgs/Bool.h>
#include <sensor_msgs/JointState.h>
#include <trajectory_msgs/JointTrajectory.h>
#include <actionlib/client/simple_action_client.h>
#include <control_msgs/FollowJointTrajectoryAction.h>
#include <geometry_msgs/Point.h>
#include <geometry_msgs/Pose.h>
#include <ur_kinematics/ur_kin.h>


// global list to hold the setposes. Not very thread safe but it's fine.
// **In ROS, Pose is a data structure composed of Point position and Quaternion
// orientation.
// Whehther list can acccept the Pose data structure is to be tested
// The answer seems to be : NO.
// Define lists for translation and rotation separately
std::list< geometry_msgs::Point > pointlist;
std::list< geometry_msgs::Quaternion > quaternionlist;

// This callback function is triggered each time that a set of pose is published.
// The only thing it does is to copy the received 6D pose to the list
// of poses
// Input: a new set pose (6D pose)
void callback( const geometry_msgs::Pose& newpose ){
    geometry_msgs::Point newpoint;
    geometry_msgs::Quaternion newquaternion;
    
    newpoint.x = newpose.position.x;
    newpoint.y = newpose.position.y;
    newpoint.z = newpose.position.z;

    newquaternion.x = newpose.orientation.x;
    newquaternion.y = newpose.orientation.y;
    newquaternion.z = newpose.orientation.z;
    newquaternion.w = newpose.orientation.w;

    std::cout << "New point\n" << newpoint << std::endl;
    std::cout << "New quaternion\n" << newquaternion << std::endl;
    pointlist.push_back( newpoint );
    quaternionlist.push_back( newquaternion );
}

// This callback_joint_states function is triggered each time when a joint state
// is published by the ur_driver. The only thing it does to initialize
// the initial joint state of the UR5 urdf model in RVIZ ??
sensor_msgs::JointState jointstate_init;
void callback_joint_states( const sensor_msgs::JointState& js )
{ jointstate_init = js; }

// actionlib
trajectory_msgs::JointTrajectory joint_trajectory;
void callback_move( const std_msgs::Bool& move ){

    static actionlib::SimpleActionClient<control_msgs::FollowJointTrajectoryAction> ac( "follow_joint_trajectory", true );

    if( move.data ){

        control_msgs::FollowJointTrajectoryGoal goal;
        goal.trajectory = joint_trajectory;
        std::cout << goal << std::endl;

        ac.sendGoal( goal );
        joint_trajectory.points.clear();

    }

}

int main( int argc, char** argv ){

    // Initialize ROS node "trajectory"
    ros::init( argc, argv, "trajectory" );

    // Create a node handle
    ros::NodeHandle nh;

    // publishing joint trajectory
    std::string published_topic_name( "/joint_trajectory" );

    // Create a publisher that will publish joint states on the topic
    // /joint_states

    // The publisher will publish to /joint_trajectory at the end of
    // of the main function after completing all the computation.
    // The size of publishing queue is 1 which means it will only buffer
    // up 1 message. The old message will be thrown away immediately
    // if a new one is received
    ros::Publisher pub_jointstate;
    pub_jointstate = nh.advertise<sensor_msgs::JointState>( published_topic_name, 1 );

    // Create a subscriber that will receive 6D setposes
    ros::Subscriber sub_setpose;
    sub_setpose = nh.subscribe( "setpose", 1, callback );

    // This is the joint state message coming from the robot
    // Get the initial joint state
    // sub_move is currently not used anywhere
    ros::Subscriber sub_move;
    sub_move = nh.subscribe( "move", 1, callback_move );

    // This is the joint state message coming from the robot
    // Get the initial joint state
    // full_joint_state is not used anywhere either
    ros::Subscriber full_joint_states;
    full_joint_states = nh.subscribe( "joint_states", 1, callback_joint_states );
    sensor_msgs::JointState jointstate;


    // To listen to the current position and orientation of the robot
    // wiki.ros.org/tf/Tutorials/Writing%20a%20tf%20listener%20%28C%2B%2B%29
    tf::TransformListener listener;
 
    // Rate (Hz) of the trajectory
    // http://wiki.ros.org/roscpp/Overview/Time
    ros::Rate rate( 100 );            // the trajectory rate
    double period = 1.0/100.0;        // the period
    double positionincrement = 0.001; // how much we move
    ros::Duration time_from_start( 0.0 );

    bool readinitpose = true;                       // used to initialize setpose
    bool moving = false;
    tf::Pose setpose;                               // the destination setpose
    tf::StampedTransform kinTrans;
    // This is the main trajectory loop
    // At each iteration it computes and publishes a new joint positions that
    // animate the motion of the robot
    // The loop exits when CTRL-C is pressed.
    while( nh.ok() ){

        // Read the current forward kinematics of the robot
        // wiki.ros.org/tf/Tutorials/Writing%20a%20tf%20listener%20%28C%2B%2B%29
        tf::Pose current_pose;
	tf::Pose trans_pose;
        try{

            // Change the name of the reference frame and target frame.
            // These names will be used by tf to return the position/orientation of
            // the last link of the robot with respect to the base of the robot
            std::string ref_frame( "base_link" );
            std::string tgt_frame( "ee_link" );

            tf::StampedTransform transform;
            listener.lookupTransform( ref_frame, tgt_frame, ros::Time::now(), transform );

            // convert tf into ROS message information
            // Note: tf is used to perform mathematicla calculation
            // while ROS message is used for communication between publisher/subscriber
            // broadcaster etc.
            current_pose.setOrigin( transform.getOrigin() );
            current_pose.setRotation( transform.getRotation() );
            /* This is used to initialize the pose (a small hack)*/
            if( readinitpose ){
                setpose = current_pose;
                readinitpose = false;
            }

        }
        catch(tf::TransformException ex)
        { std::cout << ex.what() << std::endl; }

        // If the list is not empty,
        // Read a setpose from the list and keep both the translation and rotation
        if( !pointlist.empty() && !quaternionlist.empty() ){

            // set origin of the goal pose
            double x_f = pointlist.front().x;
            double y_f = pointlist.front().y;
            double z_f = pointlist.front().z;
            setpose.setOrigin( tf::Vector3(x_f, y_f, z_f) );
            pointlist.pop_front();

            // set orientation of the goal pose
            double q_xf = quaternionlist.front().x;
            double q_yf = quaternionlist.front().y;
            double q_zf = quaternionlist.front().z;
            double q_wf = quaternionlist.front().w;
            setpose.setRotation( tf::Quaternion(q_xf, q_yf, q_zf, q_wf));
            quaternionlist.pop_front();

            moving = true;
            joint_trajectory.points.clear();
        }

        // This is the translation left for the trajectory
        tf::Point error = setpose.getOrigin() - current_pose.getOrigin();
	
        //  ** The inverse kinematics of UR5 for all six dofs is to be implemented.
        //  ** Possible solutions are ik_fast in ur_kinematics (which suffers
        //  ** from a 90% success rate), kdl_chainsolver, or the inverse kinematics
        //  ** package in me530646_lab4.

        // If the error is small enough to go to the destination
        if( moving && positionincrement < error.length() ){

            // Convert Pose to Affine3d to Affine3f to Matrix4f
            // You can see how complex it is even to convert Pose into
            // an element of SE3

	    Eigen::Affine3d H0_6d;
            tf::poseTFToEigen( setpose, H0_6d);
            Eigen::Matrix4d H_M = H0_6d.matrix();
	    Eigen::MatrixXf H_6 = H_M.cast <float> ();
	    Eigen::Matrix4f T_0;
	    T_0 << -1.0, 0.0, 0.0, 0.0,
	      0.0, -1.0, 0.0, 0.0,
	      0.0, 0.0, 1.0, 0.0, 
	      0.0, 0.0, 0.0, 1.0;
	    Eigen::Matrix4f T_f;
	    T_f << 0.0, 0.0, 1.0, 0.0,
	      -1.0, 0.0, 0.0, 0.0,
	      0.0, -1.0, 0.0, 0.0, 
	      0.0, 0.0, 0.0, 1.0;
	    H_6 = T_0*H_6*T_f;
	    double* q_sol[8];
	    for (int i = 0; i < 8; ++i) {
	      q_sol[i] = new double[6];
	    }
	    int num_sol = inverse(H_6, q_sol);
	   
	    std::vector<double> ang_dist;
	    double dists = 0;
	    for (int i = 0; i < num_sol; i++)
	    {	      
	      for (int j = 0; j < 6; j++)
	      {	
		dists += (jointstate_init.position[j] - q_sol[i][j])*(jointstate_init.position[j] - q_sol[i][j]);
	      }
	      ang_dist.push_back(sqrt(dists));
	      dists = 0;
	    }

	     //Return the iterator value for the smallest angular difference
	    int angs = std::distance(ang_dist.begin(),std::min_element(ang_dist.begin(), ang_dist.end()));
	    
            for (int i = 0; i < 6; ++i)
            {
	      //jointstate.position[i] = 0;
	      jointstate.position[i] = q_sol[angs][i];
	    }

	    tf::StampedTransform transforms;  
	    listener.lookupTransform( "base_link", "ee_link", ros::Time(0), transforms );
	    trans_pose.setOrigin(transforms.getOrigin());
	    trans_pose.setRotation(transforms.getRotation());
	    Eigen::Affine3d H0_6d_1;
            tf::poseTFToEigen( trans_pose, H0_6d_1);
            Eigen::Matrix4d H_M_1 = H0_6d_1.matrix();
	    ROS_INFO_STREAM("xt:" << H_M_1(0,3));
	    ROS_INFO_STREAM("yt:" << H_M_1(1,3));
	    ROS_INFO_STREAM("zt:" << H_M_1(2,3)); 
	   
	    //ROS_INFO_STREAM("joints" << jointstate.position[0] << "   " <<  jointstate.position[1] <<"   " << jointstate.position[2] <<"   " << jointstate.position[3] <<"   " <<jointstate.position[4] <<"   " << jointstate.position[5]);
	    jointstate_init = jointstate;

        }
        else{
            jointstate = jointstate_init;
            setpose = current_pose;
            moving = false;
        }

        // publish the joint states
        jointstate.header.stamp = ros::Time::now();
        pub_jointstate.publish( jointstate );

        //
        ros::spinOnce();

        // sleep
        rate.sleep();

    }

    return 0;

}

