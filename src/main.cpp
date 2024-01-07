#include "state-ui.h"

#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#include <chrono>
#include <thread>
#include <cmath>

namespace {
inline int64_t get_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}
}

struct VSync {
    VSync(double rate_fps = 60.0) : tStep_us(1000000.0/rate_fps) {}

    int64_t tStep_us;
    int64_t tLast_us = t_us();
    int64_t tNext_us = tLast_us + tStep_us;

    inline int64_t t_us() const {
        return get_time_us();
    }

    inline void wait() {
        int64_t tNow_us = t_us();
        while (tNow_us < tNext_us - 100) {
            std::this_thread::sleep_for(std::chrono::microseconds((int64_t) (0.9*(tNext_us - tNow_us))));
            tNow_us = t_us();
        }

        tNext_us += tStep_us;
    }

    inline float delta_s() {
        int64_t tNow_us = t_us();
        int64_t res = tNow_us - tLast_us;
        tLast_us = tNow_us;
        return float(res)/1e6f;
    }
};

struct State {
    std::string ip = "10.0.6.88";

    // port 3
    int val3 = 0;
    int pins3[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // port 5
    int val5 = 0;
    int pins5[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    int64_t t_last = 0;

    bool is_running = false;

    // execute command:
    //   $ snmpget -v1 -t 1 -r 1 -c private 192.168.1.73 enterprises.19865.1.2.1.33.0
    //   SNMPv2-SMI::enterprises.19865.1.2.1.33.0 = INTEGER: 15
    //
    // get output integer and convert to pin values
    //
    void portRead(int port) {
        const std::string obj = port == 3 ? "enterprises.19865.1.2.1.33.0" : "enterprises.19865.1.2.2.33.0";
        const std::string cmd = "snmpget -v1 -t 1 -r 1 -c private " + ip + " " + obj;

        int & val = port == 3 ? val3 : val5;
        int * pins = port == 3 ? pins3 : pins5;

        fprintf(stderr, "Running command: %s\n", cmd.c_str());

        FILE * fp = popen(cmd.c_str(), "r");
        if (fp == NULL) {
            printf("Failed to run command '%s'\n", cmd.c_str());
            return;
        }

        char buffer[1024];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            result += buffer;
        }

        pclose(fp);

        // parse result
        val = 0;
        if (sscanf(result.c_str(), ("SNMPv2-SMI::" + obj + " = INTEGER: %d").c_str(), &val) == 1) {
            for (int i = 0; i < 8; ++i) {
                pins[i] = (val >> i) & 1;

                //printf("pin %d: %d\n", i, pins[i]);
            }
        }
    }

    void portWrite(int port, int val) {
        const std::string obj = port == 3 ? "enterprises.19865.1.2.1.0.2" : "enterprises.19865.1.2.2.0.2";
        const std::string cmd = "snmpset -v1 -t 1 -r 1 -c private " + ip + " " + obj + " i " + std::to_string(val);

        fprintf(stderr, "Running command: %s\n", cmd.c_str());

        system(cmd.c_str());
    }
};

int main(int argc, char ** agrv) {
    if (argc < 1) {
        printf("Usage: %s [port]\n", agrv[0]);
        return 1;
    }

    State state;

    int port = 5015;
    const std::string httpRoot = "../static/";

    if (argc > 1) {
        port = std::stoi(agrv[1]);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.KeyMap[ImGuiKey_Tab]         = 9;
    io.KeyMap[ImGuiKey_LeftArrow]   = 37;
    io.KeyMap[ImGuiKey_RightArrow]  = 39;
    io.KeyMap[ImGuiKey_UpArrow]     = 38;
    io.KeyMap[ImGuiKey_DownArrow]   = 40;
    io.KeyMap[ImGuiKey_PageUp]      = 33;
    io.KeyMap[ImGuiKey_PageDown]    = 34;
    io.KeyMap[ImGuiKey_Home]        = 36;
    io.KeyMap[ImGuiKey_End]         = 35;
    io.KeyMap[ImGuiKey_Insert]      = 45;
    io.KeyMap[ImGuiKey_Delete]      = 46;
    io.KeyMap[ImGuiKey_Backspace]   = 8;
    io.KeyMap[ImGuiKey_Space]       = 32;
    io.KeyMap[ImGuiKey_Enter]       = 13;
    io.KeyMap[ImGuiKey_Escape]      = 27;
    io.KeyMap[ImGuiKey_A]           = 65;
    io.KeyMap[ImGuiKey_C]           = 67;
    io.KeyMap[ImGuiKey_V]           = 86;
    io.KeyMap[ImGuiKey_X]           = 88;
    io.KeyMap[ImGuiKey_Y]           = 89;
    io.KeyMap[ImGuiKey_Z]           = 90;

    io.MouseDrawCursor = true;

    ImGui::StyleColorsDark();
    //ImGui::GetStyle().AntiAliasedFill = false;
    //ImGui::GetStyle().AntiAliasedLines = false;
    //ImGui::GetStyle().WindowRounding = 0.0f;
    //ImGui::GetStyle().ScrollbarRounding = 0.0f;

    // setup imgui-ws
    ImGuiWS imguiWS;
    imguiWS.init(port, httpRoot, { "", "index.html" });

    // prepare font texture
    {
        unsigned char * pixels;
        int width, height;
        ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
        imguiWS.setTexture(0, ImGuiWS::Texture::Type::Alpha8, width, height, (const char *) pixels);
    }

    // scale everything ImGui by 2.0
    ImGui::GetStyle().ScaleAllSizes(2.0f);
    ImGui::GetIO().FontGlobalScale = 2.0f;

    VSync vsync;
    StateUI stateUI;

    while (true) {
        // websocket event handling
        auto events = imguiWS.takeEvents();
        for (auto & event : events) {
            stateUI.handle(std::move(event));
        }
        stateUI.update();

        // update state every 1 second
        if (state.is_running) {
            if (const auto t_now = get_time_us(); t_now - state.t_last > 5e6) {
                state.portRead(3);
                state.portRead(5);
                state.t_last = get_time_us();
            }
        }

        io.DisplaySize = ImVec2(600, 800);
        io.DeltaTime = vsync.delta_s();

        ImGui::NewFrame();

        //// show connected clients
        //ImGui::SetNextWindowPos({ 10, 10 } , ImGuiCond_Always);
        //ImGui::SetNextWindowSize({ 600, 300 } , ImGuiCond_Always);
        //ImGui::Begin((std::string("WebSocket clients (") + std::to_string(stateUI.clients.size()) + ")").c_str(), nullptr, ImGuiWindowFlags_NoCollapse);
        //ImGui::Text(" Id   Ip addr");
        //for (auto & [ cid, client ] : stateUI.clients) {
        //    ImGui::Text("%3d : %s", cid, client.ip.c_str());
        //    if (client.hasControl) {
        //        ImGui::SameLine();
        //        ImGui::TextDisabled(" [has control for %4.2f seconds]", stateUI.tControlNext_s - ImGui::GetTime());
        //    }
        //}
        //ImGui::End();

        ImGui::SetNextWindowPos({ 0, 0 } , ImGuiCond_Always);
        ImGui::SetNextWindowSize({ 600, 800 } , ImGuiCond_Always);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

        // input text box for the IP address
        // if state.is_running is false, then disable input
        {
            if (!state.is_running) {
                char buf[256];
                strcpy(buf, state.ip.c_str());
                ImGui::SetNextItemWidth(240);
                ImGui::InputText("##ip", buf, sizeof(buf));
                state.ip = buf;
            } else {
                ImGui::SetNextItemWidth(240);
                ImGui::InputText("##ip", (char *) state.ip.c_str(), 0, ImGuiInputTextFlags_ReadOnly);
            }

            ImGui::SameLine();
            ImGui::Text("IP address:");
        }

        // two buttons: start and stop + indicator if state.is_running is true on a single line
        {
            ImGui::SameLine();

            if (state.is_running) {
                ImGui::PushStyleColor(ImGuiCol_Button, { 0.5f, 0.0f, 0.0f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.6f, 0.0f, 0.0f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.7f, 0.0f, 0.0f, 1.0f });
                if (ImGui::Button("Stop")) {
                    state.is_running = false;
                }
                ImGui::PopStyleColor(3);
            } else {
                if (ImGui::Button("Start")) {
                    state.is_running = true;
                    state.t_last = -1e6;
                }
            }
        }

        if (state.is_running) {
            // display port 3
            {
                const auto & pins = state.pins3;

                ImGui::Text("");

                for (int i = 7; i >= 0; --i) {
                    ImGui::PushID(i);
                    ImGui::PushStyleColor(ImGuiCol_CheckMark,      pins[i] ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    ImGui::PushStyleColor(ImGuiCol_FrameBg,        pins[i] ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, pins[i] ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  pins[i] ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    bool checked = pins[i];
                    ImGui::Checkbox("##pin", &checked);
                    ImGui::PopStyleColor(4);
                    ImGui::PopID();
                    ImGui::SameLine();
                }

                ImGui::Text("Port 3");
            }

            // display port 5
            {
                const auto & pins = state.pins5;

                ImGui::Text("");

                for (int i = 7; i >= 0; --i) {
                    ImGui::PushID(i);
                    ImGui::PushStyleColor(ImGuiCol_CheckMark,      pins[i] ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    ImGui::PushStyleColor(ImGuiCol_FrameBg,        pins[i] ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, pins[i] ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  pins[i] ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    bool checked = pins[i];
                    ImGui::Checkbox("##pin", &checked);
                    ImGui::PopStyleColor(4);
                    ImGui::PopID();
                    ImGui::SameLine();
                }

                ImGui::Text("Port 5");
            }

            // draw 8 buttons in a circle
            // the first 3 pins of port 3 determine the selected button
            // 0: 0   degrees
            // 1: 45  degrees
            // 2: 90  degrees
            // 3: 135 degrees
            // 4: 180 degrees
            // 5: 225 degrees
            // 6: 270 degrees
            // 7: 315 degrees
            // print the angle below each button
            {
                const auto & val  = state.val3;
                const auto & pins = state.pins3;

                ImGui::Text("");

                const float but      = 48.0f;
                const float radius   = 200.0f;
                const float center_x = 80.0f + ImGui::GetCursorPosX() + radius;
                const float center_y = 30.0f + ImGui::GetCursorPosY() + radius;

                for (int i = 0; i < 8; ++i) {
                    const float angle = i * 45.0f * 3.14159265358979323846f / 180.0f - 3.14159265358979323846f / 2.0f;
                    const float x = center_x + radius * cos(angle);
                    const float y = center_y + radius * sin(angle);

                    const bool is_selected = ((val + 1) & 0x7) == i;

                    ImGui::SetCursorPos({ x - but / 2, y - but / 2 });
                    ImGui::PushID(i);
                    ImGui::PushStyleColor(ImGuiCol_Button,        is_selected ? ImVec4(0, 0.85, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1));
                    if (ImGui::Button("##but", { but, but })) {
                        printf("button %d selected\n", i);
                        if (i > 0) {
                            state.portWrite(3, i - 1);
                        } else {
                            state.portWrite(3, 7);
                        }
                        state.portRead(3);
                    }
                    ImGui::PopStyleColor(1);
                    ImGui::PopID();

                    {
                        const std::string txt = std::to_string(i * 45) + "°";
                        ImGui::SetCursorPos({ x + 2 - ImGui::CalcTextSize(txt.c_str()).x / 2, y + 2 + but / 2 });
                        ImGui::Text("%s", txt.c_str());
                    }
                }
            }
        }

        ImGui::End();

        // generate ImDrawData
        ImGui::Render();

        // store ImDrawData for asynchronous dispatching to WS clients
        imguiWS.setDrawData(ImGui::GetDrawData());

        // if not clients are connected, just sleep to save CPU
        do {
            vsync.wait();
        } while (imguiWS.nConnected() == 0);
    }

    ImGui::DestroyContext();

    return 0;
}
