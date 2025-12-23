import zmq
import json
import time
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
import threading
import os

class WebHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'text/html; charset=utf-8')
            self.end_headers()
            
            try:
                with open("index.html", "r", encoding="utf-8") as f:
                    html = f.read()
            except FileNotFoundError:
                html = "<h1>Файл index.html не найден</h1>"
            
            self.wfile.write(html.encode('utf-8'))
            
        elif self.path == '/data':
            self.send_response(200)
            self.send_header('Content-type', 'application/json; charset=utf-8')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            
            try:
                with open("received_data.json", "r", encoding="utf-8") as f:
                    data = json.load(f)
                    self.wfile.write(json.dumps(data, ensure_ascii=False).encode('utf-8'))
            except FileNotFoundError:
                self.wfile.write(b'[]')
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'404 Not Found')


class ZeroMQServer:
    def __init__(self, zmq_port=3333, web_port=8080):
        self.zmq_port = zmq_port
        self.web_port = web_port
        self.packet_count = 0
        self.data_file = "received_data.json"
        self.ensure_data_file()
        
    def ensure_data_file(self):
        if not os.path.exists(self.data_file):
            with open(self.data_file, "w", encoding="utf-8") as f:
                json.dump([], f, ensure_ascii=False)
    
    def save_to_file(self, data):
        try:
            with open(self.data_file, "r", encoding="utf-8") as f:
                existing_data = json.load(f)
        except FileNotFoundError:
            existing_data = []
        
        record = {
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "data": data,
            "packet_number": self.packet_count
        }
        existing_data.append(record)
        
        with open(self.data_file, "w", encoding="utf-8") as f:
            json.dump(existing_data, f, indent=2, ensure_ascii=False)
    
    def start_web_server(self):
        web_server = HTTPServer(('0.0.0.0', self.web_port), WebHandler)
        web_server.serve_forever()
    
    def run(self):
        web_thread = threading.Thread(target=self.start_web_server, daemon=True)
        web_thread.start()
        
        context = zmq.Context()
        socket = context.socket(zmq.REP)
        socket.bind(f"tcp://*:{self.zmq_port}")
        
        print(f"сервер запущен на порту {self.zmq_port}")

        
        try:
            while True:
                message = socket.recv_string()
                self.packet_count += 1
                
                print(f"Получен пакет #{self.packet_count}: {message}")
                
                self.save_to_file(message)
                
                response = "Hello from Server"
                socket.send_string(response)
                print(f"Отправлен ответ: {response}\n")
                
        except KeyboardInterrupt:
            print("\nСервер остановлен")
        finally:
            socket.close()
            context.term()


if __name__ == "__main__":
    server = ZeroMQServer(zmq_port=3333, web_port=8080)
    server.run()
