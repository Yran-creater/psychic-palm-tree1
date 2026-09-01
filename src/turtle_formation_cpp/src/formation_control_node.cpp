#include <rclcpp/rclcpp.hpp>
#include <turtle_interfaces/msg/turtle_formation_status.hpp>
#include <turtle_interfaces/srv/set_formation.hpp>
#include <turtlesim/msg/pose.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <cmath>
#include <map>
#include <algorithm>

using namespace std::chrono_literals;
using std::placeholders::_1;
using std::placeholders::_2;

class MultiTurtleFormation : public rclcpp::Node {
public:
    MultiTurtleFormation() : Node("formation_control_node") {
        // 初始化发布器
        status_pub_ = this->create_publisher<turtle_interfaces::msg::TurtleFormationStatus>("/turtle_formation/status", 10);
        cmd_vel_pubs_["turtle2"] = this->create_publisher<geometry_msgs::msg::Twist>("/turtle2/cmd_vel", 10);
        cmd_vel_pubs_["turtle3"] = this->create_publisher<geometry_msgs::msg::Twist>("/turtle3/cmd_vel", 10);

        // 初始化跟随龟位姿存储
        follower_poses_["turtle2"] = turtlesim::msg::Pose();
        follower_poses_["turtle3"] = turtlesim::msg::Pose();

        // 初始化服务
        formation_srv_ = this->create_service<turtle_interfaces::srv::SetFormation>(
            "/set_formation",
            std::bind(&MultiTurtleFormation::setFormationCallback, this, _1, _2)
        );

        // 订阅领航龟位姿
        leader_pose_sub_ = this->create_subscription<turtlesim::msg::Pose>(
            "/turtle1/pose", 10,
            std::bind(&MultiTurtleFormation::leaderPoseCallback, this, _1)
        );

        // 订阅跟随龟位姿
        for (const auto& [follower_id, _] : cmd_vel_pubs_) {
            follower_pose_subs_[follower_id] = this->create_subscription<turtlesim::msg::Pose>(
                "/" + follower_id + "/pose", 10,
                [this, follower_id](const turtlesim::msg::Pose::SharedPtr msg) {
                    follower_poses_[follower_id] = *msg;
                }
            );
        }

        // 初始化参数
        current_formation_ = "line";
        formation_dist_ = 1.5;
        linear_kp_ = 1.5;        // 增加比例系数
        angular_kp_ = 3.0;       // 增加比例系数
        max_linear_vel_ = 1.0;   
        max_angular_vel_ = 3.0;  
        
        // 添加速度死区，避免微小震荡
        linear_deadzone_ = 0.02;
        angular_deadzone_ = 0.02;
        
        RCLCPP_INFO(this->get_logger(), "多海龟编队节点启动，默认阵型：%s", current_formation_.c_str());
    }

private:
    void leaderPoseCallback(const turtlesim::msg::Pose::SharedPtr leader_pose) {
        // 发布领航龟状态
        auto status_msg = turtle_interfaces::msg::TurtleFormationStatus();
        status_msg.turtle_id = "turtle1";
        status_msg.role = "leader";
        status_msg.x = leader_pose->x;
        status_msg.y = leader_pose->y;
        status_msg.theta = leader_pose->theta;
        status_pub_->publish(status_msg);

        // 控制每个跟随龟
        int follower_index = 0;
        for (const auto& [follower_id, _] : cmd_vel_pubs_) {
            follower_index++;
            
            // 计算目标位置
            double target_x, target_y, target_theta;
            if (current_formation_ == "line") {
                // 一字型编队
                double distance = follower_index * formation_dist_;
                target_x = leader_pose->x - distance * cos(leader_pose->theta);
                target_y = leader_pose->y - distance * sin(leader_pose->theta);
                target_theta = leader_pose->theta;
            } else {  // 三角型
                if (follower_index == 1) {  // turtle2
                    target_x = leader_pose->x - formation_dist_ * cos(leader_pose->theta);
                    target_y = leader_pose->y - formation_dist_ * sin(leader_pose->theta);
                    target_theta = leader_pose->theta;
                } else {  // turtle3
                    // 改进三角阵型：在turtle2左后方或右后方
                    double angle_offset = M_PI_4;  // 45度
                    target_x = leader_pose->x - formation_dist_ * cos(leader_pose->theta + angle_offset);
                    target_y = leader_pose->y - formation_dist_ * sin(leader_pose->theta + angle_offset);
                    target_theta = leader_pose->theta + angle_offset;
                }
            }

            // 获取当前位置
            double current_x = follower_poses_[follower_id].x;
            double current_y = follower_poses_[follower_id].y;
            double current_theta = follower_poses_[follower_id].theta;

            // 计算误差
            double dx = target_x - current_x;
            double dy = target_y - current_y;
            double dist_error = std::hypot(dx, dy);
            
            // 计算目标角度（朝向目标点）
            double target_angle = std::atan2(dy, dx);
            double angle_error = target_angle - current_theta;
            
            // 角度归一化
            while (angle_error > M_PI) angle_error -= 2*M_PI;
            while (angle_error < -M_PI) angle_error += 2*M_PI;

            // 计算速度指令
            geometry_msgs::msg::Twist vel_cmd;
            
            // 改进的速度控制：先转向目标再前进
            if (std::abs(angle_error) > 0.3) {
                // 角度误差较大时，主要进行转向
                vel_cmd.linear.x = 0.0;
                vel_cmd.angular.z = angular_kp_ * angle_error;
            } else {
                // 角度误差较小时，同时进行前进和转向
                vel_cmd.linear.x = linear_kp_ * dist_error;
                vel_cmd.angular.z = angular_kp_ * angle_error;
                
                // 根据距离调整速度：距离越近速度越慢
                if (dist_error < 0.5) {
                    vel_cmd.linear.x *= (dist_error / 0.5);
                }
            }

            // 速度死区处理，防止微震荡
            if (std::abs(vel_cmd.linear.x) < linear_deadzone_) {
                vel_cmd.linear.x = 0.0;
            }
            if (std::abs(vel_cmd.angular.z) < angular_deadzone_) {
                vel_cmd.angular.z = 0.0;
            }

            // 速度限幅
            vel_cmd.linear.x = std::clamp(vel_cmd.linear.x, -max_linear_vel_, max_linear_vel_);
            vel_cmd.angular.z = std::clamp(vel_cmd.angular.z, -max_angular_vel_, max_angular_vel_);

            // 边界保护
            if (current_x < 0.5 || current_x > 10.5 || current_y < 0.5 || current_y > 10.5) {
                vel_cmd.linear.x = 0.0;
                vel_cmd.angular.z = 0.0;
                RCLCPP_WARN(this->get_logger(), "跟随龟 %s 靠近边界，紧急停止", follower_id.c_str());
            }

            // 发布速度指令
            cmd_vel_pubs_[follower_id]->publish(vel_cmd);

            // 调试信息
            if (dist_error > 0.5) {
                RCLCPP_DEBUG(this->get_logger(), 
                    "%s: 距离误差: %.2f, 角度误差: %.2f, 线速度: %.2f, 角速度: %.2f",
                    follower_id.c_str(), dist_error, angle_error, 
                    vel_cmd.linear.x, vel_cmd.angular.z);
            }

            // 发布跟随龟状态
            status_msg.turtle_id = follower_id;
            status_msg.role = "follower";
            status_msg.x = current_x;
            status_msg.y = current_y;
            status_msg.theta = current_theta;
            status_pub_->publish(status_msg);
        }
    }

    void setFormationCallback(
        const std::shared_ptr<turtle_interfaces::srv::SetFormation::Request> req,
        std::shared_ptr<turtle_interfaces::srv::SetFormation::Response> res) {
        current_formation_ = req->formation_type;
        res->success = true;
        res->message = "阵型已切换为：" + current_formation_;
        RCLCPP_INFO(this->get_logger(), res->message.c_str());
    }

    // 成员变量
    rclcpp::Publisher<turtle_interfaces::msg::TurtleFormationStatus>::SharedPtr status_pub_;
    rclcpp::Service<turtle_interfaces::srv::SetFormation>::SharedPtr formation_srv_;
    rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr leader_pose_sub_;
    std::map<std::string, rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr> follower_pose_subs_;
    std::map<std::string, rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr> cmd_vel_pubs_;
    std::map<std::string, turtlesim::msg::Pose> follower_poses_;
    
    std::string current_formation_;
    double formation_dist_;
    double linear_kp_;
    double angular_kp_;
    double max_linear_vel_;
    double max_angular_vel_;
    double linear_deadzone_;
    double angular_deadzone_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<MultiTurtleFormation>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}