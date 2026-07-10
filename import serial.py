import serial
import csv
import os
import pandas as pd


# Match this to your Arduino's serial port (e.g., 'COM3' on Windows or '/dev/ttyUSB0' on Linux)
SERIAL_PORT = 'COM5' 
BAUD_RATE = 9600

process = False
csv_file = "data.csv"
excel_file = "results.xlsx"

try:
    # Open serial connection
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

    # Create and open the CSV file in append mode
    with open("data.csv", mode="w", newline="", encoding="utf-8") as file:        
        writer = csv.writer(file)
        print("Logging started. Press Ctrl+C to stop.")    
    
        while True:
            if ser.in_waitin > 0:
                for _ in range(9):
                    print(ser.readline().strip("b'\\r\\n"))
                # Read a line of data from the Arduino
                line = ser.readline().decode('utf-8').strip()
                if line:
                    # Split the comma-separated string into a list
                    data_row = line.split(",") 
                    print(data_row)
                    
                    # Write the row to your CSV file
                    writer.writerow(data_row) 
                    file.flush() # Forces instant writing to disk
                    
except KeyboardInterrupt:
    print("Logging stopped.")
    process = True
    
except (serial.SerialException, OSError) as error:
    print(f"\n[DISCONNECTED] Serial error detected: {error}")
    should_process_excel = True
    
if process:
    print(f"Processing '{csv_file}' into Excel Writer...")

    if os.path.exists(csv_file) and os.path.getsize(csv_file) > 0:
       df = pd.read_csv(csv_file)
       df.to_excel(excel_file, sheet_name="Results", index=False)
ser.close()
