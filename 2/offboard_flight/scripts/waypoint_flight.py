#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
import time
import math
from geometry_msgs.msg import PoseStamped, Twist
from mavros_msgs.msg import State, OverrideRCIn
from mavros_msgs.srv import CommandBool, SetMode

class WaypointFlight:
    def __init__(self):
        rospy.init_node('waypoint_flight', anonymous=True)
        
        self.ns = "/solo_0"
        
        self.current_state = State()
        self.current_pose = PoseStamped()
        self.current_waypoint_index = 0
        self.position_tolerance = 0.3
        
        self.waypoints = [
            [2.0, 0.0, 3.0, 3.0],
            [2.0, 2.0, 3.0, 3.0],
            [0.0, 2.0, 3.0, 3.0],
            [0.0, 0.0, 2.0, 2.0],
        ]
        
        self.state_sub = rospy.Subscriber(
            f'{self.ns}/mavros/state', State, self.state_callback)
        self.local_pos_sub = rospy.Subscriber(
            f'{self.ns}/mavros/local_position/pose', PoseStamped, self.pose_callback)
        
        # ===== 修复：使用 Twist 而不是 TwistStamped =====
        self.vel_pub = rospy.Publisher(
            f'{self.ns}/mavros/setpoint_velocity/cmd_vel_unstamped', Twist, queue_size=10)
        
        self.rc_override_pub = rospy.Publisher(
            f'{self.ns}/mavros/rc/override', OverrideRCIn, queue_size=10)
        
        self.arm_service = rospy.ServiceProxy(
            f'{self.ns}/mavros/cmd/arming', CommandBool)
        self.set_mode_service = rospy.ServiceProxy(
            f'{self.ns}/mavros/set_mode', SetMode)
        
        self.rate = rospy.Rate(30)
        self.wait_for_connection()
        
    def state_callback(self, msg):
        self.current_state = msg
        
    def pose_callback(self, msg):
        self.current_pose = msg
        
    def wait_for_connection(self):
        rospy.loginfo(f"等待 MAVROS 连接 ({self.ns})...")
        while not rospy.is_shutdown() and not self.current_state.connected:
            self.rate.sleep()
        rospy.loginfo("MAVROS 连接成功!")
    
    def send_rc_override(self):
        rc_msg = OverrideRCIn()
        rc_msg.channels = [1500] * 18
        rc_msg.channels[2] = 1100
        rc_msg.channels[4] = 1500
        self.rc_override_pub.publish(rc_msg)
    
    def send_velocity(self, vx, vy, vz):
        """发送速度指令（使用 Twist）"""
        vel_msg = Twist()
        vel_msg.linear.x = vx
        vel_msg.linear.y = vy
        vel_msg.linear.z = vz
        self.vel_pub.publish(vel_msg)
    
    def set_offboard_mode(self):
        rospy.loginfo("切换到 Offboard 模式...")
        try:
            if self.set_mode_service(0, 'OFFBOARD'):
                rospy.loginfo("Offboard 模式切换成功")
                return True
            return False
        except Exception as e:
            rospy.logerr(f"切换失败: {e}")
            return False
    
    def arm_vehicle(self):
        rospy.loginfo("解锁无人机...")
        for i in range(20):
            self.send_rc_override()
            self.rate.sleep()
        try:
            if self.arm_service(True):
                rospy.loginfo("解锁成功")
                return True
            return False
        except Exception as e:
            rospy.logerr(f"解锁失败: {e}")
            return False
    
    def takeoff_vehicle(self, altitude=2.5):
        rospy.loginfo(f"起飞到 {altitude} 米...")
        
        # ===== 关键：先发送速度指令 =====
        rospy.loginfo("发送初始速度指令...")
        for i in range(30):
            self.send_velocity(0, 0, 0.5)
            self.send_rc_override()
            self.rate.sleep()
        
        # ===== 切换 Offboard =====
        if not self.set_offboard_mode():
            rospy.logerr("Offboard 模式切换失败")
            return False
        
        rospy.sleep(0.5)
        
        # ===== 解锁前发送速度 =====
        rospy.loginfo("解锁前发送速度指令...")
        for i in range(50):
            self.send_velocity(0, 0, 0.5)
            self.send_rc_override()
            self.rate.sleep()
        
        # ===== 解锁 =====
        if not self.arm_vehicle():
            rospy.logerr("解锁失败")
            return False
        
        # ===== 解锁后立即发送速度 =====
        rospy.loginfo("解锁后立即发送速度指令...")
        for i in range(30):
            self.send_velocity(0, 0, 0.5)
            self.send_rc_override()
            self.rate.sleep()
        
        # ===== 爬升 =====
        rospy.loginfo("爬升到目标高度...")
        timeout = 15
        while timeout > 0 and not rospy.is_shutdown():
            current_z = self.current_pose.pose.position.z
            self.send_velocity(0, 0, 0.5)
            self.send_rc_override()
            
            rospy.loginfo_throttle(2, f"当前高度: {current_z:.2f}m")
            
            if current_z >= altitude - 0.3:
                rospy.loginfo(f"到达目标高度 {altitude:.1f}m")
                break
            
            self.rate.sleep()
            timeout -= 0.033
        
        # ===== 悬停 =====
        rospy.loginfo("悬停中...")
        for i in range(30):
            self.send_velocity(0, 0, 0)
            self.send_rc_override()
            self.rate.sleep()
        
        return True
    
    def fly_to_waypoint(self, target_x, target_y, target_z):
        rospy.loginfo(f"飞向 ({target_x}, {target_y}, {target_z})...")
        
        timeout = 30
        while timeout > 0 and not rospy.is_shutdown():
            current = self.current_pose.pose.position
            
            dx = target_x - current.x
            dy = target_y - current.y
            dz = target_z - current.z
            
            distance = math.sqrt(dx*dx + dy*dy + dz*dz)
            
            if distance < self.position_tolerance:
                rospy.loginfo("到达航点")
                break
            
            gain = 0.8
            vx = min(max(dx * gain, -1.0), 1.0)
            vy = min(max(dy * gain, -1.0), 1.0)
            vz = min(max(dz * gain, -0.5), 0.5)
            
            self.send_velocity(vx, vy, vz)
            self.send_rc_override()
            self.rate.sleep()
            timeout -= 0.033
        
        for i in range(20):
            self.send_velocity(0, 0, 0)
            self.send_rc_override()
            self.rate.sleep()
        
        return True
    
    def land_vehicle(self):
        rospy.loginfo("开始降落...")
        
        while not rospy.is_shutdown():
            current_z = self.current_pose.pose.position.z
            if current_z < 0.2:
                rospy.loginfo("已降落")
                break
            
            speed = -0.3 if current_z > 1.0 else -0.15
            self.send_velocity(0, 0, speed)
            self.send_rc_override()
            self.rate.sleep()
        
        return True
    
    def execute_mission(self):
        rospy.loginfo("开始执行任务...")
        
        rospy.Timer(rospy.Duration(0.05), lambda event: self.send_rc_override())
        
        rospy.loginfo("等待 EKF 收敛...")
        time.sleep(3)
        
        if not self.takeoff_vehicle(2.5):
            rospy.logerr("起飞失败")
            return False
        
        rospy.loginfo("起飞完成，开始航点任务...")
        
        for i, wp in enumerate(self.waypoints):
            rospy.loginfo(f"航点 {i+1}: ({wp[0]}, {wp[1]}, {wp[2]})")
            
            if not self.fly_to_waypoint(wp[0], wp[1], wp[2]):
                rospy.logerr(f"航点 {i+1} 失败")
                break
            
            rospy.loginfo(f"悬停 {wp[3]} 秒...")
            start_time = time.time()
            while time.time() - start_time < wp[3] and not rospy.is_shutdown():
                self.send_velocity(0, 0, 0)
                self.send_rc_override()
                self.rate.sleep()
        
        self.land_vehicle()
        rospy.loginfo("任务完成!")
        return True

def main():
    try:
        flight = WaypointFlight()
        flight.execute_mission()
        rospy.loginfo("✅ 任务成功完成")
    except rospy.ROSInterruptException:
        pass
    except Exception as e:
        rospy.logerr(f"发生错误: {e}")

if __name__ == '__main__':
    main()
