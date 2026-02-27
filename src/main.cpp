#include <SDL2/SDL.h>
#include <GL/glew.h>

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <zmq.hpp>
#include "implot.h"

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"

#define NETWORK_PORT 5555

struct GeoData {
    std::atomic<float> latValue;
    std::atomic<float> lonValue;
    std::atomic<float> altValue;
    std::atomic<long long> timeStamp;
    std::string deviceId;
    std::string cellInfo;
};

void logMessage(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm now_tm = *std::localtime(&now_time_t);
    
    std::cout << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") 
              << "." << std::setfill('0') << std::setw(3) << now_ms.count()
              << "] [" << level << "] " << message << std::endl;
}

void parseIncomingData(const std::string& rawData, GeoData* geoInfo) {
    try {
        logMessage("DEBUG", "Парсинг полученных данных: " + rawData);
        
        size_t latPos = rawData.find("\"latitude\"");
        size_t lonPos = rawData.find("\"longitude\"");
        size_t altPos = rawData.find("\"altitude\"");
        size_t tsPos = rawData.find("\"timestamp\"");
        size_t imeiPos = rawData.find("\"imei\"");
        size_t cellInfoPos = rawData.find("\"cellInfo\"");
        if (latPos != std::string::npos) {
            size_t colonPos = rawData.find(":", latPos);
            size_t commaPos = rawData.find(",", colonPos);
            std::string latStr = rawData.substr(colonPos + 1, commaPos - colonPos - 1);
            geoInfo->latValue.store(std::stof(latStr));
            logMessage("DEBUG", "Широта: " + latStr);
        }
        
        if (lonPos != std::string::npos) {
            size_t colonPos = rawData.find(":", lonPos);
            size_t commaPos = rawData.find(",", colonPos);
            std::string lonStr = rawData.substr(colonPos + 1, commaPos - colonPos - 1);
            geoInfo->lonValue.store(std::stof(lonStr));
            logMessage("DEBUG", "Долгота: " + lonStr);
        }
        
        if (altPos != std::string::npos) {
            size_t colonPos = rawData.find(":", altPos);
            size_t commaPos = rawData.find(",", colonPos);
            if (commaPos == std::string::npos) commaPos = rawData.find("}", colonPos);
            std::string altStr = rawData.substr(colonPos + 1, commaPos - colonPos - 1);
            geoInfo->altValue.store(std::stof(altStr));
            logMessage("DEBUG", "Высота: " + altStr);
        }

        if (tsPos != std::string::npos) {
            size_t colonPos = rawData.find(":", tsPos);
            size_t commaPos = rawData.find(",", colonPos);
            if (commaPos == std::string::npos) commaPos = rawData.find("}", colonPos);
            std::string tsStr = rawData.substr(colonPos + 1, commaPos - colonPos - 1);
            geoInfo->timeStamp.store(std::stoll(tsStr));
            logMessage("DEBUG", "Время: " + tsStr);
        }

        if (imeiPos != std::string::npos) {
            size_t colonPos = rawData.find(":", imeiPos);
            size_t commaPos = rawData.find(",", colonPos);
            if (commaPos == std::string::npos) commaPos = rawData.find("}", colonPos);
            std::string imeiStr = rawData.substr(colonPos + 1, commaPos - colonPos - 1);
            if (imeiStr.size() >= 2 && imeiStr.front() == '"' && imeiStr.back() == '"') {
                imeiStr = imeiStr.substr(1, imeiStr.size() - 2);
            }
            geoInfo->deviceId = imeiStr;
            logMessage("DEBUG", "IMEI: " + imeiStr);
        }

        if (cellInfoPos != std::string::npos) {
            size_t colonPos = rawData.find(":", cellInfoPos);
            size_t endPos = rawData.find(",", colonPos + 1);
            if (endPos == std::string::npos) {
                endPos = rawData.find("}", colonPos + 1);
            }
            std::string cellInfoStr = rawData.substr(colonPos + 1, endPos - colonPos - 1);
            if (cellInfoStr.size() >= 2 && cellInfoStr.front() == '"' && cellInfoStr.back() == '"') {
                cellInfoStr = cellInfoStr.substr(1, cellInfoStr.size() - 2);
            }
            geoInfo->cellInfo = cellInfoStr;
            logMessage("DEBUG", "Cell Info получена, длина: " + std::to_string(cellInfoStr.length()));
        }
        
        logMessage("INFO", "Данные успешно распознаны");
    } catch (const std::exception& e) {
        logMessage("ERROR", "Ошибка при парсинге данных: " + std::string(e.what()));
    } catch (...) {
        logMessage("ERROR", "Неизвестная ошибка при парсинге данных");
    }
}

void displayInterface(GeoData* geoInfo) {
    logMessage("INFO", "Инициализация графического интерфейса");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        logMessage("ERROR", "SDL_Init Error: " + std::string(SDL_GetError()));
        return;
    }
    logMessage("DEBUG", "SDL инициализирован успешно");
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Geo Data Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, windowFlags);
    
    if (!window) {
        logMessage("ERROR", "Не удалось создать окно: " + std::string(SDL_GetError()));
        return;
    }
    logMessage("DEBUG", "Окно создано успешно");
    
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        logMessage("ERROR", "Не удалось создать контекст OpenGL: " + std::string(SDL_GetError()));
        return;
    }
    
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        logMessage("ERROR", "Failed to initialize GLEW: " + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewStatus))));
        return;
    }
    logMessage("DEBUG", "GLEW инициализирован успешно");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImPlot::CreateContext();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.WindowTitleAlign = ImVec2(-0.1f, 0.5f);
    style.WindowPadding = ImVec2(15, 15);
    style.ItemSpacing = ImVec2(5, 5);
    style.FramePadding = ImVec2(6, 4);
    
    ImVec4 clear_color = ImVec4(0.05f, 0.63f, 0.31f, 1.00f);

    if (!ImGui_ImplSDL2_InitForOpenGL(window, glContext)) {
        logMessage("ERROR", "Не удалось инициализировать ImGui SDL2");
        return;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        logMessage("ERROR", "Не удалось инициализировать ImGui OpenGL3");
        return;
    }
    
    logMessage("INFO", "Графический интерфейс инициализирован успешно");

    bool isRunning = true;
    
    while (isRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                isRunning = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && 
                event.window.windowID == SDL_GetWindowID(window))
                isRunning = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.7f));
        
        ImGui::Begin("Location Information", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        try {
            ImGui::Text("Latitude: %.6f°", geoInfo->latValue.load());
            ImGui::Text("Longitude: %.6f°", geoInfo->lonValue.load());
            ImGui::Text("Altitude: %.2f meters", geoInfo->altValue.load());
            ImGui::Text("Timestamp: %lld", geoInfo->timeStamp.load());
            ImGui::Text("Device IMEI: %s", geoInfo->deviceId.c_str());
            
            ImGui::Separator();
            if (!geoInfo->cellInfo.empty()) {
                ImGui::Text("Cell Info: получена (длина: %zu символов)", geoInfo->cellInfo.length());
                std::string preview = geoInfo->cellInfo.substr(0, 100) + "...";
                ImGui::TextWrapped("Preview: %s", preview.c_str());
            } else {
                ImGui::Text("Cell Info: нет данных");
            }
        } catch (const std::exception& e) {
            logMessage("ERROR", "Ошибка при отображении данных: " + std::string(e.what()));
        }
        
        ImGui::Separator();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / io.Framerate, io.Framerate);
        
        ImGui::Spacing();
        
        ImGui::End();
        ImGui::PopStyleColor();
        
        ImGui::Begin("Plots", nullptr, ImGuiWindowFlags_None);

        ImVec2 available_size = ImGui::GetContentRegionAvail();

        if (ImPlot::BeginPlot("Line Plots", available_size)) {
            ImPlot::SetupAxes("x", "y");
            
            static float xs1[1001], ys1[1001];
            for (int i = 0; i < 1001; ++i) {
                xs1[i] = i * 0.001f;
                ys1[i] = 0.5f + 0.5f * sinf(50 * (xs1[i] + (float)ImGui::GetTime() / 10));
            }
            static double xs2[20], ys2[20];
            for (int i = 0; i < 20; ++i) {
                xs2[i] = i * 1/19.0f;
                ys2[i] = xs2[i] * xs2[i];
            }
            
            ImPlot::PlotLine("f(x)", xs1, ys1, 1001);
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
            ImPlot::PlotLine("g(x)", xs2, ys2, 20, ImPlotLineFlags_Segments);
            
            ImPlot::EndPlot();
        }

        ImGui::End();

        ImGui::Render();
        
        int displayWidth, displayHeight;
        SDL_GetWindowSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    logMessage("INFO", "Завершение работы графического интерфейса");
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    logMessage("INFO", "Графический интерфейс завершен");
}

void startNetworkServer(GeoData* geoInfo) {
    logMessage("INFO", "Запуск сетевого сервера на порту " + std::to_string(NETWORK_PORT));
    
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_REP);
        
        std::string bindAddress = "tcp://*:" + std::to_string(NETWORK_PORT);
        logMessage("DEBUG", "Привязка к адресу: " + bindAddress);
        
        socket.bind(bindAddress);
        logMessage("INFO", "Сервер успешно запущен и ожидает подключений");

        struct stat dirStat = {0};
        if (stat("../database", &dirStat) == -1) {
            if (mkdir("../database", 0700) == 0) {
                logMessage("INFO", "Создана директория ../database");
            } else {
                logMessage("ERROR", "Не удалось создать директорию ../database");
            }
        }

        while (true) {
            try {
                zmq::message_t request;
                logMessage("DEBUG", "Ожидание входящего сообщения...");
                
                auto receiveResult = socket.recv(request, zmq::recv_flags::none);
                
                if (receiveResult) {
                    std::string receivedData(static_cast<char*>(request.data()), request.size());
                    logMessage("INFO", "Получены данные от клиента");
                    
                    parseIncomingData(receivedData, geoInfo);

                    std::string filename = "../database/locations.json";
                    
                    std::ifstream checkFile(filename);
                    bool fileExists = checkFile.good();
                    checkFile.close();

                    std::ofstream file(filename, std::ios::app);
                    
                    if (file.is_open()) {
                        if (!fileExists) {
                            file << "[\n";
                            file << receivedData << "\n";
                            file << "]";
                            logMessage("DEBUG", "Создан новый файл с данными");
                        } else {
                            std::ifstream readFile(filename);
                            std::stringstream content;
                            content << readFile.rdbuf();
                            readFile.close();
                            
                            std::string fileContent = content.str();
                            
                            if (fileContent.length() > 1 && 
                                fileContent.substr(fileContent.length() - 2) == "\n]") {
                                fileContent = fileContent.substr(0, fileContent.length() - 2);
                                
                                std::ofstream writeFile(filename);
                                writeFile << fileContent;
                                
                                if (fileContent.length() > 2) {
                                    writeFile << ",\n";
                                }

                                writeFile << receivedData << "\n";
                                writeFile << "]";
                                writeFile.close();
                                logMessage("DEBUG", "Данные добавлены в существующий файл");
                            } else {
                                logMessage("WARNING", "Файл поврежден, создается новый");
                                std::ofstream newFile(filename);
                                newFile << "[\n";
                                newFile << receivedData << "\n";
                                newFile << "]";
                                newFile.close();
                            }
                        }
                        file.close();
                        
                        logMessage("INFO", "Данные успешно сохранены в файл");
                        
                        std::string response = "Successful sending";
                        socket.send(zmq::buffer(response), zmq::send_flags::none);
                        logMessage("DEBUG", "Отправлен ответ клиенту: " + response);
                        
                    } else {
                        logMessage("ERROR", "Не удалось открыть файл для записи");
                        std::string response = "Unexpected error";
                        socket.send(zmq::buffer(response), zmq::send_flags::none);
                    }
                } else {
                    logMessage("ERROR", "Ошибка при получении данных");
                }
                
            } catch (const zmq::error_t& e) {
                logMessage("ERROR", "Ошибка ZeroMQ: " + std::string(e.what()));
            } catch (const std::exception& e) {
                logMessage("ERROR", "Стандартная ошибка: " + std::string(e.what()));  
            } catch (...) {
                logMessage("ERROR", "Неизвестная ошибка в цикле обработки");
            }
        }
    } catch (const zmq::error_t& e) {
        logMessage("ERROR", "Критическая ошибка ZeroMQ при запуске сервера: " + std::string(e.what()));
    } catch (const std::exception& e) {
        logMessage("ERROR", "Критическая стандартная ошибка: " + std::string(e.what()));
    } catch (...) {
        logMessage("ERROR", "Критическая неизвестная ошибка при запуске сервера");
    }
}

int main() {
    logMessage("INFO", "Запуск приложения");
    
    GeoData geoInfo;
    
    try {
        geoInfo.latValue.store(0.0f);
        geoInfo.lonValue.store(0.0f);
        geoInfo.altValue.store(0.0f);
        geoInfo.timeStamp.store(0);
        geoInfo.deviceId = "None";
        geoInfo.cellInfo = ""; 
        
        logMessage("DEBUG", "Структура данных инициализирована");
        
        logMessage("INFO", "Запуск серверного потока");
        std::thread serverThread(startNetworkServer, &geoInfo);
        
        logMessage("INFO", "Запуск графического интерфейса");
        displayInterface(&geoInfo);
        
        logMessage("INFO", "Ожидание завершения серверного потока");
        serverThread.join();
        
    } catch (const std::exception& e) {
        logMessage("ERROR", "Ошибка в main: " + std::string(e.what()));
        return 1;
    } catch (...) {
        logMessage("ERROR", "Неизвестная ошибка в main");
        return 1;
    }
    
    logMessage("INFO", "Приложение завершено");
    return 0;
}