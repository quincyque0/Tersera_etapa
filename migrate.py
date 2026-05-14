import json
import psycopg2
import re
from datetime import datetime

DB_CONFIG = {
    'host': 'localhost',
    'database': 'cell_monitor',
    'user': 'postgres',
    'password': 'postgres'
}

def parse_cell_info(cell_info_str):
    cells = []
    lines = cell_info_str.strip().split('\n')
    for line in lines:
        if not line.strip():
            continue
        
        cell = {}
        rssi_match = re.search(r'rssi=(-?\d+)', line)
        if rssi_match:
            cell['rssi'] = int(rssi_match.group(1))

        rsrp_match = re.search(r'rsrp=(-?\d+)', line)
        if rsrp_match:
            cell['rsrp'] = int(rsrp_match.group(1))
        else:
            cell['rsrp'] = 0

        rsrq_match = re.search(r'rsrq=(-?\d+)', line)
        if rsrq_match:
            cell['rsrq'] = int(rsrq_match.group(1))
        else:
            cell['rsrq'] = 0

        arfcn_match = re.search(r'mArfcn=(\d+)', line)
        if arfcn_match:
            cell['arfcn'] = int(arfcn_match.group(1))

        cid_match = re.search(r'mCid=(\d+)', line)
        if cid_match:
            cell['cell_id'] = int(cid_match.group(1))

        lac_match = re.search(r'mLac=(\d+)', line)
        if lac_match:
            cell['lac'] = int(lac_match.group(1))

        cell['is_registered'] = 'mRegistered=YES' in line

        operator_match = re.search(r'mAlphaLong="([^"]+)"', line)
        if operator_match:
            cell['operator_name'] = operator_match.group(1)
        
        if cell:
            cells.append(cell)
    
    return cells

def clear_database(cursor):
    cursor.execute("DROP TABLE IF EXISTS cell_info CASCADE")
    cursor.execute("DROP TABLE IF EXISTS data_points CASCADE")
    print("Старые таблицы удалены")

def create_tables(cursor):
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS data_points (
            id SERIAL PRIMARY KEY,
            timestamp BIGINT NOT NULL,
            latitude REAL NOT NULL,
            longitude REAL NOT NULL,
            altitude REAL NOT NULL,
            device_id VARCHAR(50) NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS cell_info (
            id SERIAL PRIMARY KEY,
            data_point_id INTEGER REFERENCES data_points(id) ON DELETE CASCADE,
            rssi INTEGER NOT NULL,
            arfcn INTEGER NOT NULL,
            cell_id INTEGER NOT NULL,
            lac INTEGER NOT NULL,
            is_registered BOOLEAN DEFAULT FALSE,
            operator_name VARCHAR(100),
            timestamp BIGINT NOT NULL,
            rsrp INTEGER DEFAULT 0,
            rsrq INTEGER DEFAULT 0
        )
    """)
    
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_data_points_timestamp ON data_points(timestamp)")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_data_points_lat_lon ON data_points(latitude, longitude)")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_cell_info_cell_id ON cell_info(cell_id)")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_cell_info_data_point_id ON cell_info(data_point_id)")
    
    print("Таблицы созданы с индексами")

def migrate_data():
    json_file = 'database/locations.json'
    try:
        with open(json_file, 'r') as f:
            content = f.read()
            data = json.loads(content)
    except FileNotFoundError:
        print(f"Файл {json_file} не найден!")
        return False
    except json.JSONDecodeError as e:
        print(f"Ошибка парсинга JSON: {e}")
        return False
    
    print(f"Найдено {len(data)} записей в JSON файле")
    
    try:
        conn = psycopg2.connect(**DB_CONFIG)
        cursor = conn.cursor()
        print("Подключено к PostgreSQL")
    except Exception as e:
        print(f"Ошибка подключения к PostgreSQL: {e}")
        print("Убедитесь, что PostgreSQL запущен и база данных создана")
        return False
    
    clear_database(cursor)
    create_tables(cursor)
    conn.commit()
    
    migrated = 0
    errors = 0
    
    for idx, record in enumerate(data):
        try:
            cursor.execute("""
                INSERT INTO data_points (timestamp, latitude, longitude, altitude, device_id)
                VALUES (%s, %s, %s, %s, %s)
                RETURNING id
            """, (
                record.get('timestamp', 0),
                record.get('latitude', 0.0),
                record.get('longitude', 0.0),
                record.get('altitude', 0.0),
                record.get('imei', 'unknown')
            ))
            
            data_point_id = cursor.fetchone()[0]
            
            cell_info_str = record.get('cellInfo', '')
            if cell_info_str:
                cells = parse_cell_info(cell_info_str)
                for cell in cells:
                    cursor.execute("""
                        INSERT INTO cell_info 
                        (data_point_id, rssi, arfcn, cell_id, lac, is_registered, operator_name, timestamp, rsrp, rsrq)
                        VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
                    """, (
                        data_point_id,
                        cell.get('rssi', 0),
                        cell.get('arfcn', 0),
                        cell.get('cell_id', 0),
                        cell.get('lac', 0),
                        cell.get('is_registered', False),
                        cell.get('operator_name', ''),
                        record.get('timestamp', 0),
                        cell.get('rsrp', 0),
                        cell.get('rsrq', 0)
                    ))
            
            migrated += 1
            
            if (idx + 1) % 100 == 0:
                print(f"Перенесено {migrated} записей")
                conn.commit()
                
        except Exception as e:
            print(f"Ошибка в записи {idx}: {e}")
            errors += 1
    
    conn.commit()
    
    cursor.execute("SELECT COUNT(*) FROM data_points")
    total_points = cursor.fetchone()[0]
    
    cursor.execute("SELECT COUNT(DISTINCT cell_id) FROM cell_info")
    unique_cells = cursor.fetchone()[0]
    
    cursor.execute("SELECT MIN(latitude), MAX(latitude), MIN(longitude), MAX(longitude) FROM data_points")
    bounds = cursor.fetchone()
    
    cursor.close()
    conn.close()
    

    print(f"Успешно перенесено: {migrated} записей")
    print(f"Ошибок: {errors}")
    print(f"Всего точек в БД: {total_points}")
    print(f"Уникальных сотовых вышек: {unique_cells}")
    print(f"Границы карты:")
    print(f"  Широта: от {bounds[0]:.4f} до {bounds[1]:.4f}")
    print(f"  Долгота: от {bounds[2]:.4f} до {bounds[3]:.4f}")

    
    return migrated > 0

if __name__ == "__main__":
    if migrate_data():
        print("\nПеренос данных завершен успешно")
    else:
        print("\nПеренос данных не выполнен")
