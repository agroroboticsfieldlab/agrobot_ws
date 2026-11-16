// GearedODriveHardwareInterface
//
// ros2_control SystemInterface that wraps the unmodified ODrive plugin from ros_odrive
// (odrive_ros2_control_plugin/ODriveHardwareInterface) and adds a per-joint gear ratio.
//
// The original plugin is loaded through pluginlib and does all CAN communication. This wrapper only
// converts between joint units (what ROS sees) and ODrive units (what the original plugin sees):
//
//   ODrive position/velocity = joint position/velocity * gear_ratio
//   ODrive torque            = joint torque / gear_ratio
//
// Joint parameters (in addition to the ones of the original plugin, e.g. node_id):
//   gear_ratio  (optional, default 1.0, must be > 0): ODrive turns per joint turn.
//
// Hardware parameters are passed through unchanged (e.g. "can").

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "pluginlib/class_loader.hpp"
#include "rclcpp/rclcpp.hpp"

namespace agrobot_odrive {

using hardware_interface::CallbackReturn;
using hardware_interface::return_type;

class GearedODriveHardwareInterface final : public hardware_interface::SystemInterface {
public:
    using State = rclcpp_lifecycle::State;

    GearedODriveHardwareInterface()
        : loader_("hardware_interface", "hardware_interface::SystemInterface") {}

    CallbackReturn on_init(const hardware_interface::HardwareComponentInterfaceParams& params) override;
    CallbackReturn on_configure(const State& previous_state) override { return inner_->on_configure(previous_state); }
    CallbackReturn on_cleanup(const State& previous_state) override { return inner_->on_cleanup(previous_state); }
    CallbackReturn on_shutdown(const State& previous_state) override { return inner_->on_shutdown(previous_state); }
    CallbackReturn on_activate(const State& previous_state) override { return inner_->on_activate(previous_state); }
    CallbackReturn on_deactivate(const State& previous_state) override { return inner_->on_deactivate(previous_state); }
    CallbackReturn on_error(const State& previous_state) override { return inner_->on_error(previous_state); }

    std::vector<hardware_interface::StateInterface> export_state_interfaces() override;
    std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

    return_type prepare_command_mode_switch(
        const std::vector<std::string>& start_interfaces,
        const std::vector<std::string>& stop_interfaces
    ) override {
        return inner_->prepare_command_mode_switch(start_interfaces, stop_interfaces);
    }
    return_type perform_command_mode_switch(
        const std::vector<std::string>& start_interfaces,
        const std::vector<std::string>& stop_interfaces
    ) override {
        return inner_->perform_command_mode_switch(start_interfaces, stop_interfaces);
    }

    return_type read(const rclcpp::Time& time, const rclcpp::Duration& period) override;
    return_type write(const rclcpp::Time& time, const rclcpp::Duration& period) override;

private:
    // Joint-side values exposed to ros2_control, plus the matching handles of the inner plugin.
    struct Joint {
        std::string name;
        double gear_ratio = 1.0;

        double pos_state = NAN;
        double vel_state = NAN;
        double effort_state = NAN;
        double pos_cmd = 0.0;
        double vel_cmd = 0.0;
        double effort_cmd = 0.0;

        hardware_interface::StateInterface* inner_pos_state = nullptr;
        hardware_interface::StateInterface* inner_vel_state = nullptr;
        hardware_interface::StateInterface* inner_effort_state = nullptr;
        hardware_interface::CommandInterface* inner_pos_cmd = nullptr;
        hardware_interface::CommandInterface* inner_vel_cmd = nullptr;
        hardware_interface::CommandInterface* inner_effort_cmd = nullptr;
    };

    template <typename T>
    static T* find_handle(std::vector<T>& handles, const std::string& joint, const std::string& interface);

    static constexpr const char* kInnerPlugin = "odrive_ros2_control_plugin/ODriveHardwareInterface";

    // The loader must outlive the plugin instance, so it is declared first.
    pluginlib::ClassLoader<hardware_interface::SystemInterface> loader_;
    std::shared_ptr<hardware_interface::SystemInterface> inner_;
    std::vector<hardware_interface::StateInterface> inner_state_interfaces_;
    std::vector<hardware_interface::CommandInterface> inner_command_interfaces_;
    std::vector<Joint> joints_;
};

CallbackReturn GearedODriveHardwareInterface::on_init(const hardware_interface::HardwareComponentInterfaceParams& params) {
    if (hardware_interface::SystemInterface::on_init(params) != CallbackReturn::SUCCESS) {
        return CallbackReturn::ERROR;
    }

    for (const auto& joint_info : info_.joints) {
        Joint joint;
        joint.name = joint_info.name;
        if (auto it = joint_info.parameters.find("gear_ratio"); it != joint_info.parameters.end()) {
            try {
                joint.gear_ratio = std::stod(it->second);
            } catch (const std::exception&) {
                joint.gear_ratio = NAN;
            }
        }
        if (!(joint.gear_ratio > 0.0) || !std::isfinite(joint.gear_ratio)) {
            RCLCPP_ERROR(get_logger(), "Invalid gear_ratio for joint %s, must be a number > 0", joint.name.c_str());
            return CallbackReturn::ERROR;
        }
        RCLCPP_INFO(get_logger(), "Joint %s: gear_ratio %g", joint.name.c_str(), joint.gear_ratio);
        joints_.push_back(joint);
    }

    try {
        inner_ = loader_.createSharedInstance(kInnerPlugin);
    } catch (const pluginlib::PluginlibException& e) {
        RCLCPP_ERROR(get_logger(), "Failed to load %s: %s", kInnerPlugin, e.what());
        return CallbackReturn::ERROR;
    }

    // The inner plugin gets the same description. It ignores gear_ratio. It gets its own name so
    // that its internal node does not clash with the node of this component.
    hardware_interface::HardwareComponentParams inner_params;
    inner_params.hardware_info = info_;
    inner_params.hardware_info.name = info_.name + "_odrive";
    inner_params.logger = get_logger().get_child("odrive");
    inner_params.clock = get_clock();
    inner_params.executor = params.executor;
    if (inner_->init(inner_params) != CallbackReturn::SUCCESS) {
        RCLCPP_ERROR(get_logger(), "Failed to initialize %s", kInnerPlugin);
        return CallbackReturn::ERROR;
    }

    // The original plugin uses the (deprecated) pointer-based export API.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    inner_state_interfaces_ = inner_->export_state_interfaces();
    inner_command_interfaces_ = inner_->export_command_interfaces();
#pragma GCC diagnostic pop

    for (auto& joint : joints_) {
        joint.inner_pos_state = find_handle(inner_state_interfaces_, joint.name, hardware_interface::HW_IF_POSITION);
        joint.inner_vel_state = find_handle(inner_state_interfaces_, joint.name, hardware_interface::HW_IF_VELOCITY);
        joint.inner_effort_state = find_handle(inner_state_interfaces_, joint.name, hardware_interface::HW_IF_EFFORT);
        joint.inner_pos_cmd = find_handle(inner_command_interfaces_, joint.name, hardware_interface::HW_IF_POSITION);
        joint.inner_vel_cmd = find_handle(inner_command_interfaces_, joint.name, hardware_interface::HW_IF_VELOCITY);
        joint.inner_effort_cmd = find_handle(inner_command_interfaces_, joint.name, hardware_interface::HW_IF_EFFORT);
        if (!joint.inner_pos_state || !joint.inner_vel_state || !joint.inner_effort_state || !joint.inner_pos_cmd
            || !joint.inner_vel_cmd || !joint.inner_effort_cmd) {
            RCLCPP_ERROR(get_logger(), "%s does not export the expected interfaces for joint %s", kInnerPlugin, joint.name.c_str());
            return CallbackReturn::ERROR;
        }
    }

    return CallbackReturn::SUCCESS;
}

template <typename T>
T* GearedODriveHardwareInterface::find_handle(std::vector<T>& handles, const std::string& joint, const std::string& interface) {
    for (auto& handle : handles) {
        if (handle.get_prefix_name() == joint && handle.get_interface_name() == interface) {
            return &handle;
        }
    }
    return nullptr;
}

// Same interfaces, in the same order, as the original plugin.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
std::vector<hardware_interface::StateInterface> GearedODriveHardwareInterface::export_state_interfaces() {
    std::vector<hardware_interface::StateInterface> state_interfaces;
    for (auto& joint : joints_) {
        state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_EFFORT, &joint.effort_state);
        state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_VELOCITY, &joint.vel_state);
        state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_POSITION, &joint.pos_state);
    }
    return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> GearedODriveHardwareInterface::export_command_interfaces() {
    std::vector<hardware_interface::CommandInterface> command_interfaces;
    for (auto& joint : joints_) {
        command_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_EFFORT, &joint.effort_cmd);
        command_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_VELOCITY, &joint.vel_cmd);
        command_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_POSITION, &joint.pos_cmd);
    }
    return command_interfaces;
}
#pragma GCC diagnostic pop

return_type GearedODriveHardwareInterface::read(const rclcpp::Time& time, const rclcpp::Duration& period) {
    const return_type result = inner_->read(time, period);

    // The inner handles are only used from this thread, so get_optional() never fails on the lock.
    for (auto& joint : joints_) {
        joint.pos_state = joint.inner_pos_state->get_optional().value_or(NAN) / joint.gear_ratio;
        joint.vel_state = joint.inner_vel_state->get_optional().value_or(NAN) / joint.gear_ratio;
        joint.effort_state = joint.inner_effort_state->get_optional().value_or(NAN) * joint.gear_ratio;
    }

    return result;
}

return_type GearedODriveHardwareInterface::write(const rclcpp::Time& time, const rclcpp::Duration& period) {
    for (auto& joint : joints_) {
        if (!joint.inner_pos_cmd->set_value(joint.pos_cmd * joint.gear_ratio)
            || !joint.inner_vel_cmd->set_value(joint.vel_cmd * joint.gear_ratio)
            || !joint.inner_effort_cmd->set_value(joint.effort_cmd / joint.gear_ratio)) {
            RCLCPP_ERROR(get_logger(), "Failed to pass command of joint %s to the ODrive plugin", joint.name.c_str());
            return return_type::ERROR;
        }
    }

    return inner_->write(time, period);
}

} // namespace agrobot_odrive

PLUGINLIB_EXPORT_CLASS(agrobot_odrive::GearedODriveHardwareInterface, hardware_interface::SystemInterface)
