#!/usr/bin/env python
import roslib; roslib.load_manifest('active_echo_simul_keyboard')
import rospy
import tf

from active_echo_serial.msg import Num 

import sys, select, termios, tty

msg = """
Reading from the keyboard  and Publishing to active_echo_data!
---------------------------
Moving around:
u    i
j    k
m    ,

q/z : increase/decrease l_ta changing speed by 10%
w/x : increase/decrease dly  changing speed by 10%
e/c : increase/decrease tc   changing speed by 10%
anything else : stop

CTRL-C to quit
"""

moveBindings = {

        'u':  0.001,
        'i': -0.001,
        'j':  0.001,
        'k': -0.001,
        'm':  0.001,
        ',': -0.001,
        'o':  0.000,
        }

def getKey():
    tty.setraw(sys.stdin.fileno())
    select.select([sys.stdin], [], [], 0)
    key = sys.stdin.read(1)
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
    return key

# initial position of the active echo point
x = 0.4 # m
y = 0.4 # m
z = 0.1 # m

# print the three member variables of active_echo_data
def params(x, y, z):
    return "Initial position of the AE elmemnt is:\tx = %s \ty = %s \tz = %s" % (x,y,z)

if __name__=="__main__":
    settings = termios.tcgetattr(sys.stdin)
    # pub = rospy.Publisher('active_echo_data', Num, queue_size = 5)
    rospy.init_node('active_echo_signal_simul_keyboard')
    br = tf.TransformBroadcaster()

    try:
        print params(x, y, z)
        while(1):
            
            key = getKey()
            if key in moveBindings.keys():
                if key == 'u' or key == 'i':
                    x = x + moveBindings[key]
                elif key == 'j' or key == 'k':
                    y = y + moveBindings[key]
                elif key == 'm' or key == ',':
                    z = z + moveBindings[key]
                elif key == 'o':
                    print "AE element static"
                else:
                    print 'Not valid key stroke'
            else:
                if (key == '\x03'):
                    break
            br.sendTransform((x,y,z), 
                    tf.transformations.quaternion_from_euler(0,0,0),
                    rospy.Time.now(),
                    "active_echo_position",
                    "world")
    except:
        print 'Unexpected error in try'

    finally:
        br.sendTransform((x,y,z), 
                tf.transformations.quaternion_from_euler(0,0,0),
                rospy.Time.now(),
                "active_echo_position",
                "world")
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
