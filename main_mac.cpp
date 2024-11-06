#include "neeblemancy.cpp"

int main() {
    srand(time(0));
    bool good_input = false;
    std::cout << "\n\033[95mNEEBLEMANCY\033[0m v0.2 (Windows)\n© 2024 WCMay\n";
    std::cout << "\n\033[36m2\033[0m/\033[36m3\033[0m/\033[36m4\033[0m:  Start game with 2/3/4 players\n\033[36mh\033[0m:      Help\n\033[36mc\033[0m:      Credits\n";
    try {
        while(!good_input){
            std::cout << "\n>> ";
            std::string input;
            std::cin >> input;
            if (input.size() > 1) throw(input);
            switch(input[0]) {
                case 'h':
                    std::cout << "\nBINDINGS\n"
                                  "          LKeys     RKeys     Controller\n"
                                  "Move       \033[36mW/S       O/L       LStick\n\033[0m"
                                  "Look       \033[36mA/D       K/;       RStick\n\033[0m"
                                  "Shoot      \033[36mE         I         RTrigger\n\033[0m"
                                  "Recall     \033[36mQ         P         RBumper\n\033[0m"
                                  "Phase      \033[36mLShift    RShift    LTrigger/LBumper\n\n\033[0m"
                                  "OTHER KEYS\n"
                                  "\033[36m1/2/3/4     \033[0mChange Player 1/2/3/4 Controller\n"
                                  "\033[36m9           \033[0mNew Game\n"
                                  "\033[36mDown Arrow  \033[0mQuit Game\n";
                    break;
                case 'c':
                    std::cout << "\nNEEBLEMANCY v0.2\n"
                                 "© 2024 WCMay\n"
                                 "Created by \033[95mBOSH\033[0m, Neeblemancy Archmage\n"
                                 "Libraries: cow.cpp, OpenGL, GLFW\n"
                                 "All assets are original\n"
                                 "No neebles were harmed in the making of this game\n"
                                 "Special Thanks to JMB!\n";
                    break;
                case 'n':
                case 'N':
                    std::cout << "\n   ████\n"
                                 "█ █████\n"
                                 "██████ ██\n"
                                 "  █ ███ █\n"
                                 "  █████\n"
                                 "  ████\n"
                                 "  █  █\n"
                                 "  █  █\n"
                                 " █   █\n";
                    break;
                default:
                    NUM_PLAYERS = input[0] - '0';
                    if (NUM_PLAYERS < 2 || NUM_PLAYERS > 4) throw (GAMEMODE);
                    if (NUM_PLAYERS > 2) {
                        std::cout << "\nTeams? (\033[36my\033[0m/\033[36mn\033[0m)\n";
                        std::cout << "\n>> ";
                        char boolchar;
                        std::cin >> boolchar;
                        if (boolchar == 'y') TEAM_MODE = true;
                        else if (boolchar == 'n') TEAM_MODE = false;
                        else throw(GAMEMODE);
                    }
                    else TEAM_MODE = false;
                    std::cout << "\nGamemodes:\n\033[36m1\033[0m:   Classic\n\033[36m2\033[0m:   Haunted Hotel\n";
                    std::cout << "\n>> ";
                    std::cin >> GAMEMODE;
                    GAMEMODE -= 1;
                    if (GAMEMODE == -1 || GAMEMODE > 1) throw(GAMEMODE);
                    good_input = true;
                    break;
            }
        }
    }
    catch(...) {
        std::cout << "\nInvalid input.\n";
        GAMEMODE = 0;
        good_input = false;
    }
    if (good_input) {
        std::cout << "\nStarting game...\n\n";
        initialize_game();
        APPS {
            APP(NEEBLEMANCY);
            //APP(app1);
            //APP(carl);
        }
    }
}
