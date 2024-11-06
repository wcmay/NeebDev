#include "neeblemancy.cpp"

int main() {
    srand(time(0));
    bool good_input = false;
    std::cout << "\nNEEBLEMANCY v0.2 (Windows)\n(c) 2024 WCMay\n";
    std::cout << "\n2/3/4:  Start game with 2/3/4 players\nh:      Help\nc:      Credits\n";
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
                                  "Move       W/S       O/L       LStick\n"
                                  "Look       A/D       K/;       RStick\n"
                                  "Shoot      E         I         RTrigger\n"
                                  "Recall     Q         P         RBumper\n"
                                  "Phase      LShift    RShift    LTrigger/LBumper\n\n"
                                  "OTHER KEYS\n"
                                  "1/2/3/4     Change Player 1/2/3/4 Controller\n"
                                  "9           New Game\n"
                                  "Down Arrow  Quit Game\n";
                    break;
                case 'c':
                    std::cout << "\nNEEBLEMANCY v0.2\n"
                                 "(c) 2024 WCMay\n"
                                 "Created by BOSH, Neeblemancy Archmage\n"
                                 "Libraries: cow.cpp, OpenGL, GLFW\n"
                                 "All assets are original\n"
                                 "No neebles were harmed in the making of this game\n"
                                 "Special Thanks to JMB!\n";
                    break;
                case 'n':
                case 'N':
                    std::cout << "\n   @@@@\n"
                                 "@ @@@@@\n"
                                 "@@@@@@ @@\n"
                                 "  @ @@@ @\n"
                                 "  @@@@@\n"
                                 "  @@@@\n"
                                 "  @  @\n"
                                 "  @  @\n"
                                 " @   @\n";
                    break;
                default:
                    NUM_PLAYERS = input[0] - '0';
                    if (NUM_PLAYERS < 2 || NUM_PLAYERS > 4) throw (GAMEMODE);
                    if (NUM_PLAYERS > 2) {
                        std::cout << "\nTeams? (y/n)\n";
                        std::cout << "\n>> ";
                        char boolchar;
                        std::cin >> boolchar;
                        if (boolchar == 'y') TEAM_MODE = true;
                        else if (boolchar == 'n') TEAM_MODE = false;
                        else throw(GAMEMODE);
                    }
                    else TEAM_MODE = false;
                    std::cout << "\nGamemodes:\n1:   Classic\n2:   Haunted Hotel\n";
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
