/**

    ===========XBOX CONTROLLER GUIDE===========

    Axis 0          LStick L (-1.0) to R (1.0)
    Axis 1          LStick U (-1.0) to D (1.0)
    Axis 2          RStick L (-1.0) to R (1.0)
    Axis 3          RStick U (-1.0) to D (1.0)
    Axis 4          RTrigger U (-1.0) to D (1.0)
    Axis 5          LTrigger U (-1.0) to D (1.0)

    Button 00       A
    Button 01       B
    Button 02       
    Button 03       X
    Button 04       Y
    Button 05       
    Button 06       LBump
    Button 07       RBump
    Button 08       
    Button 09       
    Button 10       
    Button 11       LMenu 
    Button 12       
    Button 13       LStick
    Button 14       RStick
    Button 15       XBox Central Button
    Button 16       RMenu
    Button 17       DPad U
    Button 18       DPad R
    Button 19       DPad D
    Button 20       DPad L

**/



void carl() {
    while (cow_begin_frame()) {
        int num_axes;
        int num_buttons;
        int num_hats;
        const float *axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &num_axes);
        const unsigned char *buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &num_buttons);
        const unsigned char *hats = glfwGetJoystickHats(GLFW_JOYSTICK_1, &num_hats);
        { // gui_readout controller
            for (int i = 0; i < num_axes; ++i) {
                real tmp = axes[i];
                char buffer[16]; sprintf(buffer, "axis %d", i);
                gui_readout(buffer, &tmp);
            }
            for (int i = 0; i < num_buttons; ++i) {
                int tmp = buttons[i];
                char buffer[16]; sprintf(buffer, "button %d", i);
                gui_readout(buffer, &tmp);
            }
            for (int i = 0; i < num_hats; ++i) {
                int tmp = hats[i];
                char buffer[16]; sprintf(buffer, "hat %d", i);
                gui_readout(buffer, &tmp);
            }
        }
    }
}
/**
int main() {
    APPS {
        APP(carl);
    }
    return 0;
}**/
