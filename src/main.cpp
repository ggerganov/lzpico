#include "state-ui.h"

#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#include <chrono>
#include <thread>

struct VSync {
    VSync(double rate_fps = 60.0) : tStep_us(1000000.0/rate_fps) {}

    uint64_t tStep_us;
    uint64_t tLast_us = t_us();
    uint64_t tNext_us = tLast_us + tStep_us;

    inline uint64_t t_us() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count(); // duh ..
    }

    inline void wait() {
        uint64_t tNow_us = t_us();
        while (tNow_us < tNext_us - 100) {
            std::this_thread::sleep_for(std::chrono::microseconds((uint64_t) (0.9*(tNext_us - tNow_us))));
            tNow_us = t_us();
        }

        tNext_us += tStep_us;
    }

    inline float delta_s() {
        uint64_t tNow_us = t_us();
        uint64_t res = tNow_us - tLast_us;
        tLast_us = tNow_us;
        return float(res)/1e6f;
    }
};

struct State {
    std::string ip = "192.168.1.73";

    int pins[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    void readPort() {
        // execute command:
        //   $ snmpget -v1 -t 1 -r 1 -c private 192.168.1.73 enterprises.19865.1.2.1.33.0
        //   SNMPv2-SMI::enterprises.19865.1.2.1.33.0 = INTEGER: 15
        //
        // get output integer and convert to pin values

        const std::string cmd = "snmpget -v1 -t 1 -r 1 -c private " + ip + " enterprises.19865.1.2.1.33.0";

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
        int val = 0;
        if (sscanf(result.c_str(), "SNMPv2-SMI::enterprises.19865.1.2.1.33.0 = INTEGER: %d", &val) == 1) {
            for (int i = 0; i < 8; ++i) {
                pins[i] = (val >> i) & 1;

                printf("pin %d: %d\n", i, pins[i]);
            }
        }
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

    VSync vsync;
    StateUI stateUI;

    while (true) {
        // websocket event handling
        auto events = imguiWS.takeEvents();
        for (auto & event : events) {
            stateUI.handle(std::move(event));
        }
        stateUI.update();

        io.DisplaySize = ImVec2(1200, 800);
        io.DeltaTime = vsync.delta_s();

        ImGui::NewFrame();

        //// show connected clients
        //ImGui::SetNextWindowPos({ 10, 10 } , ImGuiCond_Always);
        //ImGui::SetNextWindowSize({ 400, 300 } , ImGuiCond_Always);
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

        ImGui::SetNextWindowPos({ 8, 4 } , ImGuiCond_Always);
        ImGui::SetNextWindowSize({ 800, 400 } , ImGuiCond_Always);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);

        // input text box for the IP address
        {
            char buf[256];
            strcpy(buf, state.ip.c_str());
            ImGui::InputText("IP address", buf, sizeof(buf));
            state.ip = buf;
        }

        if (ImGui::Button("Update")) {
            state.readPort();
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
