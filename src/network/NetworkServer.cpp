#include "../network/NetworkServer.h"
#include "../parser/JsonParser.h"
#include "../utils/Logger.h"
#include <zmq.hpp>

NetworkServer::NetworkServer(GeoData* geoInfo) : geoInfo(geoInfo), running(true) {}

NetworkServer::~NetworkServer() {
    stop();
}

void NetworkServer::setStorage(std::shared_ptr<PostgresStorage> storage) {
    this->storage = storage;
}

void NetworkServer::processDataPoint(const DataPoint& point) {
    if (!point.cells.empty()) {
        if (storage && storage->isConnected()) {
            storage->saveDataPoint(point);
        }
        geoInfo->addDataPoint(point);
        
        Logger::log("INFO", "Получено " + std::to_string(point.cells.size()) + " сот");
    }
}

void NetworkServer::parseIncomingData(const std::string& rawData) {
    try {
        DataPoint newPoint = JsonParser::parseDataPoint(rawData);
        processDataPoint(newPoint);
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка парсинга: " + std::string(e.what()));
    }
}

void NetworkServer::start() {
    Logger::log("INFO", "ZeroMQ сервер на порту " + std::to_string(NETWORK_PORT));
    
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_REP);
        
        socket.bind("tcp://*:" + std::to_string(NETWORK_PORT));
        Logger::log("INFO", "Сервер запущен");
        
        while (running) {
            zmq::message_t request;
            auto result = socket.recv(request, zmq::recv_flags::none);
            
            if (result) {
                std::string data(static_cast<char*>(request.data()), request.size());
                parseIncomingData(data);
                socket.send(zmq::buffer("OK"), zmq::send_flags::none);
            }
        }
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка сервера: " + std::string(e.what()));
    }
}

void NetworkServer::stop() {
    running = false;
}