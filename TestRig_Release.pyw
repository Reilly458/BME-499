# TODO
# - Fix GUI element variable casing
# - Fix updateRightUI()

import tkinter as tk
from tkinter import ttk
import tkinter.messagebox as tkMsg
from tkinter.filedialog import asksaveasfile
import tkinter.font as tkFont
import logging
import os
import json
import serial
import serial.tools.list_ports
import threading
import time
import copy
import csv
import crc
import struct
import pandas as pd
#import DataProcessing as dp
from csvAnalytics import *
from excelWriter import *
from excelWriter import decog_stuck_flag

DATAPATH = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', 'Database'))
RESULTSPATH = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', 'Test Results'))
RIGPATH = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', 'Rig Configs'))
GRIDPADX = 5
GRIDPADY = 2

global_serial_object = None     # need this so that the serial object outlives runTestEventHandler
global_run_thread_flag = False  # indicates when the serial receive thread should stop after a test
decog_stuck_flag  = [0] * 10

# Update if any new fields get added to test profiles or rig configs
profile_dict = {
    'actuator': 
    {
        'model':    '',
        'motor':    0,
        'stroke':   0,
        'gear':     0,
        'voltage':  0,
        'type':     ''
    },
    'rig':  
    {
        'rewind':   0.0,
        'decog':    0,
        'maglim':   0,
        'extlim':   0,
        'retlim':   0,
        'reel':     ''
    },  
    'test': 
    {
        'duty':     0,
        'speed':    0,
        'extend':   0,
        'retract':  0,
        'format':   '',
        'startload':0,
        'endload':  0,
        'runs':     0,
        'findmax':  0,
        'warmup':   0,
        'reduced_speed': 0   #new addition        
    },
    'brake':
    {
        'size': 0,
        'A': [],
        'B': [],
        'C': [],
        'D': [],
        'F': [],
        'T': [],
        'max': [],
        'min': [],
        'rewind_coeff': 0.0
    },
    'loadcell': 
    {
        'A_high': 0.0,
        'B_high': 0.0,
        'C_high': 0.0,
        'D_high': 0.0,
        'E_high': 0.0,
        'threshold': 0,
        'A_low': 0.0,
        'B_low': 0.0,
        'C_low': 0.0,
        'D_low': 0.0,
        'E_low': 0.0,
        'max_load': 0
    },
    'posSensor':
    {
        'scale': 0
    },
    'tic':
    {
        'maxaccel':   0,
        'startspeed': 0,
        'maxcurrent': 0,
        'stepmode': 0,
        'stepsize': 0.0,
        'maxdecel' : 0     #new addition
    }
}

# Update with any new rig profiles that get made
def abbreviate_rig_name(full_name: str) -> str:
    # As named in the rig config .json files
    if full_name == 'bigrig': return 'BR'
    elif full_name == 'original': return 'MR'
    elif full_name == 'smallrig': return 'SR'
    else: return ''
    
def swap_endianness(arr: bytearray, wordsize: int) -> None:
    for i in range(0, len(arr), wordsize):
        arr[i:i+wordsize] = arr[i:i+wordsize][::-1]   # need to flip each 4-byte word to match STM32 CRC

def connect(port, baud):
    """
    The function initiates the connection to the controller with the provided COM port and baud.

    args:
        port: COM port to controller
        baud: Serial symbol rate

    returns: 
        Open serial pbject
    """

    try:
        serial_object = serial.Serial(port, baud)
    except ValueError:
        print('Enter Baud and Port')
        serial_object = None
    except IOError:
        print('Could not open serial port')
        serial_object = None

    return serial_object
    

def getDataThread(serial_object):
    """
    This function serves the purpose of collecting data from the serial object and storing 
    the filtered data into a global variable.

    The function has been put into a thread since the serial event is a blocking function.

    args:
        serial_object: An open serial connection to controller
    """
    
    global global_run_thread_flag

    header = 'time,run,target load,pwm,position,force,current'
    fname = 'error.csv'
    run = ''
    step_mode = 0
    load = 0
    pwm = 0
    speed_cmd = 0
    aquire = False
    
    while global_run_thread_flag:   
        try:
            #serial_object.readline()
            serial_data = serial_object.readline()
            
            filter_data = str(serial_data).strip("b'\\r\\n")
            print(filter_data)

            if 'AMP TEST - ' in filter_data:
                fname = 'ramp_data.csv'
                with open(fname, 'w', newline='') as f:
                    f.write(header + '\n')
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Run: ' + run
            elif 'TEPPER SPEED TEST - ' in filter_data:
                fname = 'stepper_speed_data.csv'
                with open(fname, 'w', newline='') as f:
                    f.write('time,run,step mode,target speed,target load,position,force,current,speed cmd,stall\n')
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Run: ' + run
            elif 'TEPPER STALL TEST - ' in filter_data:
                fname = 'stepper_stall_data.csv'
                with open(fname, 'w', newline='') as f:
                    f.write('time,run,step mode,target speed,target load,position,force,current,speed cmd,stall\n')
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Run: ' + run
            elif 'IND MAX FORCE TEST' in filter_data:
                fname = 'maxf_data.csv'
                with open(fname, 'w', newline='') as f:
                    f.write(header + '\n')
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Run: ' + run
            elif 'OADCELL CHECK - ' in filter_data:
                fname = 'loadcell_data.csv'
                with open(fname, 'w', newline='') as f:
                    f.write(header + '\n')
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Run: ' + run
            elif 'URRENT CHECK' in filter_data:
                fname = 'current_data.csv'
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Run: ' + run
            elif 'TALL CURRENT TEST' in filter_data:
                fname = 'stall_data.csv'
                with open(fname, 'w', newline='') as f:
                    f.write(header + '\n')                
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Run: 1'
            elif 'ELEMETRY LOGGER' in filter_data:
                fname = 'telemetry.csv'
                with open(fname, 'w', newline='') as f:
                    f.write(header + '\n')
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Run: ' + run
            elif 'DECOG ROUTINE' in filter_data:
                outlog.delete('1.0', '500.0')
                outlog.insert('1.0', filter_data + '\n')
                labelr['text'] = 'Decog'
            elif 'END OF TESTS' in filter_data:
                labelr['text'] = ' '
                popupMsg('All Tests Complete... Processing Data')
            elif '####' in filter_data and profile_dict['actuator']['motor'] == 'STEPPER':
                outlog.delete('2.0', '500.0')
                outlog.insert('2.0', '\n' + filter_data + ' (Run Number,Step Mode,Target Speed,Target Load,Speed Command)\n')
                run = filter_data.split(',')[1]
                step_mode = filter_data.split(',')[2]
                load = filter_data.split(',')[3]
                pwm = filter_data.split(',')[4]
                speed_cmd = filter_data.split(',')[5]
                labelr['text'] = 'Run: ' + run
                aquire = True
            elif '####' in filter_data:     # this condition is for brushed motor testing and calibrations
                outlog.delete('2.0','500.0')
                outlog.insert('2.0','\n'+filter_data+' (Run Number,Target Load,Load PWM)'+'\n')
                run = filter_data.split(",")[1]
                load = filter_data.split(",")[2]
                pwm = filter_data.split(",")[3]
                labelr['text'] = 'Run: '+run
                aquire = True
            elif '!' in filter_data:
                outlog.delete('2.0', '500.0')
                outlog.insert('2.0', '\n' + filter_data + '\n')
                labelr['text'] = ''
                aquire = False
                if 'STALLED' in filter_data and profile_dict['actuator']['motor'] == 'STEPPER':
                    filter_data = filter_data.split(',')
                    csv_data = (('%s,' * 9 + '1')  # 9 strings, followed by a 1 to indicate stall
                                %(filter_data[1], run, step_mode, load, pwm, filter_data[2], filter_data[3], filter_data[4], speed_cmd))
                    with open(fname, 'a', newline='') as f:
                        f.write(csv_data.strip('"') + '\n')
                elif 'MAX FORCE' in filter_data:
                    filter_data = filter_data.split(',')
                    csv_data = (('%s,' * 9 + '1')  # 9 strings, followed by a 1 to indicate stall
                                %(filter_data[2], run, step_mode, load, filter_data[1], filter_data[3], filter_data[4], filter_data[5], speed_cmd))
                    with open(fname, 'a', newline='') as f:
                        f.write(csv_data.strip('"') + '\n')  
                ### Reilly addition for stuck in decog detection ###         
                elif 'decog' in filter_data and profile_dict['actuator']['motor'] == 'STEPPER':   
                    global decog_stuck_flag
                    decog_stuck_flag[int(run)-1] = True
                ### End Reilly addition ###
            elif aquire is True: #actual csv data
                outlog.insert('2.0', '\n' + filter_data)
                filter_data = filter_data.split(',') 
                csv_data = (('%s,' * 9 + '0')  # 9 strings, followed by a 0 to indicate no stall
                            %(filter_data[0], run, step_mode, load, pwm, filter_data[1], filter_data[2], filter_data[3], speed_cmd)
                            if profile_dict['actuator']['motor'] == 'STEPPER' else 
                            filter_data[0]+","+run+","+load+","+pwm+","+filter_data[1]+","+filter_data[2]+","+filter_data[3])
                with open(fname, 'a', newline='') as f:
                    f.write(csv_data.strip('"') + '\n')
                          
        except serial.SerialException as error:
            print('Serial error: %s' %(error))
            global_run_thread_flag = False
            global_serial_object.close()
        

def startProcessing():
    """
    Handles the processing of data at the end of a test run
    """

    if profile_dict['test']['format'] == 'RAMP':
        csvFile = ('ramp_data.csv')
        
    model = profile_dict['actuator']['model'][:-2]
    rigName = abbreviate_rig_name(rig_cb.get())
    fname = '%s\\%s_%s_%s.xlsx' %(RESULTSPATH, rigName, model, time.strftime('%Y-%m-%d %H-%M-%S'))
    note = test_note.get()
    
    try:
        if profile_dict['actuator']['model'] == 'CALIBRATION':
            fname = RESULTSPATH + '\\' +'CALIBRATION '+ time.strftime('%Y-%m-%d %H-%M-%S') + '.xlsx'
            data = calProcessor('loadcell_data.csv')
            calDoc = excelDoc(fname)
            calCheck = calCheckSheet(calDoc, data.dfCal, data.curResult, data.forceResult)
            calDoc.saveDoc()
        elif profile_dict['test']['format'] == 'RAMP':
            data = rampProcessor(csvFile, profile_dict)
            Doc = excelDoc(fname)
            ramp = rampSheet(Doc, data.dfAvg, data.dfSpecs, data.checkUp, note)
            
            #Generate individual run data in report if requested
            if detailOP.get() == '1' or detailOP.get() == 1:
                for i, run in enumerate(data.dfRuns):
                    runSheet(Doc, run, data.avgTruncList[i],i+1)
            Doc.saveDoc()
        elif profile_dict['test']['format'] == 'STEPPER SPEED':
            doc = excelDoc(fname)
            data = stepperSpeedProcessor('stepper_speed_data.csv', profile_dict)
            stepperSpeedSheet(doc, data.df, data.dfAvg, note, decog_stuck_flag)
            #Generate individual run data in report if requested
            if detailOP.get() == '1' or detailOP.get() == 1:
                [stepperRunSheet(doc, data.dfRuns[n], data.avgTruncList[n]) for n in range(data.dfAvg.shape[0])]
            doc.saveDoc()
        elif profile_dict['test']['format'] == 'STEPPER STALL':
            doc = excelDoc(fname)

            data = stepperSpeedProcessor('stepper_stall_data.csv', profile_dict)
            stepperSpeedSheet(doc, data.df, data.dfAvg, note, decog_stuck_flag)
            #Generate individual run data in report if requested
            if detailOP.get() == '1' or detailOP.get() == 1:
                [stepperRunSheet(doc, data.dfRuns[n], data.avgTruncList[n]) for n in range(data.dfAvg.shape[0])]
            doc.saveDoc()
            
        else:
            pass

    except IndexError as e:
        print('There was a problem parsing the csv data: ' + str(e))
        

def validateJson() -> bool:
    
    ### TODO: fill this in with criteria to validate test parameters
    
    return True


def transcribeJson() -> bytearray:
    """
    Convert the JSON strings to the appropriate enums and stringifies the data

    returns:
        Data in string form, zero-padded for consistent transmission size
    """

    num_profile = copy.deepcopy(profile_dict)

    print('TRANSCRIBE')
    print(num_profile)
    
    # Update with new motor types/test formats as they get added
    str_to_num = {
        'BRUSHED': 1,
        'STEPPER': 2,
        'A': 1,
        'B': 2,
        'C': 3,
        'D': 4,
        'E': 5,
        'NONE': 0,
        'RAMP': 1,
        'STEPPER SPEED': 2,
        'STEPPER STALL': 3,
        'LOGGER': 4,
        'DECOGGER': 5
    }
    try:
        num_profile["actuator"]["motor"]=str_to_num[num_profile["actuator"]["motor"]]
        num_profile["rig"]["reel"]=str_to_num[num_profile["rig"]["reel"]]
        num_profile["test"]["format"]=str_to_num[num_profile["test"]["format"]]
    except:
        pass
    
    # build up data packet by looping through entire profile dictionary
    data = b''
    for _, category in num_profile.items():
        for _, value in category.items():
            if type(value) is int:
                data += value.to_bytes(4, 'little')
            elif type(value) is float:
                data += struct.pack('<f', value)
            elif type(value) is str:
                data += value.zfill(16).encode()
            elif type(value) is list:
                for f in value: data += struct.pack('<f', f)

    return data


def sendData(serial_object, chars):
    """
    Helper function to simplify transmitting serial data
    
    args:
        serial_object: An open serial connection to controller
        chars: Data to transmit
    """

    serial_object.write(bytes(chars, 'utf-8'))


def modifyJson():
    """
    Updates the profile dictionary based on user GUI selections.  Prints the updates to the console.
    """

    profile_dict['actuator'].update({'model': modelNumber.get()})
    profile_dict['actuator'].update({'motor': motorType.get()})
    profile_dict['actuator'].update({'stroke': int(strokeLength.get())})
    profile_dict['actuator'].update({'gear': int(gear.get())})
    profile_dict['actuator'].update({'voltage': int(voltage.get())})
    profile_dict['actuator'].update({'type': actType.get()})

    profile_dict['rig'].update({'rewind': float(rewindSpeed.get())})
    profile_dict['rig'].update({'decog': int(decog.get())})
    profile_dict['rig'].update({'maglim': int(maglim.get())})
    profile_dict['rig'].update({'extlim': int(extlim.get())})
    profile_dict['rig'].update({'retlim': int(retlim.get())})
    profile_dict['rig'].update({'reel': reel.get()})
    profile_dict['rig'].update({'hangwt': int(hang.get())})

    profile_dict['test'].update({'duty': int(dutyCycle.get())})
    profile_dict['test'].update({'speed': int(speed.get())})
    profile_dict['test'].update({'extend': int(extend.get())})
    profile_dict['test'].update({'retract': int(retract.get())})
    profile_dict['test'].update({'format': selected_test.get()})
    profile_dict['test'].update({'startload': int(startLoad.get())})
    profile_dict['test'].update({'endload': int(endLoad.get())})
    profile_dict['test'].update({'runs': int(numberRuns.get())})
    profile_dict['test'].update({'warmup': int(warmup.get())})
    profile_dict['test'].update({'findmax': int(findMax.get())})
    profile_dict['test'].update({'stallcrnt': int(stallCurrent.get())})
    profile_dict['test'].update({'noload': int(noLoad.get())})
    
    if motorType.get() == 'STEPPER':
        profile_dict['tic'].update({'maxaccel': int(stepAccel.get())})
        if 'maxdecel' in profile_dict['tic']:
            profile_dict['tic'].update({'maxdecel': int(stepDecel.get())})
        profile_dict['tic'].update({'startspeed': int(stepSpeed.get())})
        profile_dict['tic'].update({'maxcurrent': int(stepCurrent.get())})
        profile_dict['tic'].update({'stepsize': float(stepSize.get())})
        profile_dict['tic'].update({'stepmode': int(stepMode.get())})
        if 'reduced_speed' in profile_dict['test']:
            profile_dict['test'].update({'reduced_speed': int(reducedSpeed.get())})        

    print(profile_dict)


def sendCsv(serial_object):
    """
    Loads stepper test data from the selected CSV file and transmits it to the controller
    
    args:
        serial_object: An open serial connection to controller
    """
    
    with open(DATAPATH + '\\' + step_test_cb.get(), 'r') as csv_file:
        reader = csv.DictReader(csv_file)
        count = sum(1 for row in reader)    # scan file once to get number of tests
        csv_file.seek(0)                    # reset back to start of file
        reader = pd.read_csv(csv_file)      # now we can iterate through each entry
        
        serial_object.write(count.to_bytes(1, 'little'))     # number of entries needed so controller can allocate memory
        data = b''
        for entry in reader['step (1/2^x)']:
            data += int(entry).to_bytes(4, 'little')
        for entry in reader['speed (mm/s)']:
            data += int(entry).to_bytes(4, 'little')
        for entry in reader['load (N)']:
            data += struct.pack('<f', float(entry))
        serial_object.write(data)


### These 3 function need to be looked at and possibly changed
def popupMsg(msg):
    global global_run_thread_flag

    popup = tk.Tk()
    popup.wm_title('!')
    label = ttk.Label(popup, text=msg)
    label.pack(side='top', fill='x', pady=10)
    print('\a')
    B1 = ttk.Button(popup, text='Close', command = popup.destroy)
    B2 = ttk.Button(popup, text='Detailed Output', command = generateDetail)
    B2.pack()
    B1.pack()

    global_run_thread_flag = False  # signals the serial receive thread to stop
    global_serial_object.close()

    startProcessing()
    
    popup.mainloop()
  

 
def popupCal(serial_object):
    popup = tk.Tk()
    popup.wm_title('!')
    label = ttk.Label(popup, text='Add next load')
    label.pack(side='top', fill='x', pady=10) 
    print('\a')
    B1 = ttk.Button(popup, text='Close', command = popup.destroy)
    B2 = ttk.Button(popup, text='Continue', command = lambda: sendData(serial_object, '\n'))
    B2.pack()
    B1.pack()
    popup.mainloop()


def generateDetail():
    detailOP.set('1')
    startProcessing()
###

def loadProfileEventHandler(event):
    """
    Loads a new test profile from the selected JSON file into the app.  This function is called when the user selects a new test profile.

    Args:
        event: Contains event arguments sent by Tkinter 
    """

    with open(DATAPATH+'\\'+event.widget.get()+'.json') as f:
        profile_dict.update(json.load(f))
    
    modelNumber.set(profile_dict['actuator']['model'])
    motorType.set(profile_dict['actuator']['motor'])
    strokeLength.set(profile_dict['actuator']['stroke'])
    gear.set(profile_dict['actuator']['gear'])
    voltage.set(profile_dict['actuator']['voltage'])
    actType.set(profile_dict['actuator']['type'])

    rewindSpeed.set(profile_dict['rig']['rewind'])
    decog.set(profile_dict['rig']['decog'])
    maglim.set(profile_dict['rig']['maglim'])
    extlim.set(profile_dict['rig']['extlim'])
    retlim.set(profile_dict['rig']['retlim'])
    reel.set(profile_dict['rig']['reel'])
    hang.set(profile_dict['rig']['hangwt'])

    dutyCycle.set(profile_dict['test']['duty'])
    speed.set(profile_dict['test']['speed'])   
    extend.set(profile_dict['test']['extend'])
    retract.set(profile_dict['test']['retract'])
    test_cb.set(profile_dict['test']['format'])
    startLoad.set(profile_dict['test']['startload'])
    endLoad.set(profile_dict['test']['endload'])
    dutyCycle.set(profile_dict['test']['duty'])
    numberRuns.set(profile_dict['test']['runs'])
    warmup.set(profile_dict['test']['warmup'])
    findMax.set(profile_dict['test']['findmax'])
    stallCurrent.set(profile_dict['test']['stallcrnt'])
    noLoad.set(profile_dict['test']['noload'])
    
    if motorType.get() == 'STEPPER':
        stepAccel.set(profile_dict['tic']['maxaccel'])
        if 'maxdecel' in profile_dict['tic']:
            stepDecel.set(profile_dict['tic']['maxdecel'])      #new addition
        else:
            profile_dict['tic']['maxdecel'] = profile_dict['tic']['maxaccel']
            stepDecel.set(profile_dict['tic']['maxdecel'])      #new addition
        stepSpeed.set(profile_dict['tic']['startspeed'])
        stepCurrent.set(profile_dict['tic']['maxcurrent'])
        stepSize.set(profile_dict['tic']['stepsize'])
        stepMode.set(profile_dict['tic']['stepmode'])
        if 'reduced_speed' in profile_dict['test']:
            reducedSpeed.set(profile_dict['test']['reduced_speed'])  #new addition
        else:
            profile_dict['test']['reduced_speed'] = 0
            reducedSpeed.set(profile_dict['test']['reduced_speed'])  #new addition
    if 'maxdecel' not in profile_dict['tic']:
        profile_dict['tic']['maxdecel'] = profile_dict['tic']['maxaccel']
        #stepDecel.set(profile_dict['tic']['maxdecel'])      #new addition
    if 'reduced_speed' not in profile_dict['test']:
        profile_dict['test']['reduced_speed'] = 0
        #reducedSpeed.set(profile_dict['test']['reduced_speed'])  #new addition
    updateRightUI(None)
    print(profile_dict)

def loadRigEventHandler(event):
    """
    Loads a new rig config from the selected JSON file into the app.  This function is called when the user selects a new rig config.

    Args:
        event: Contains event arguments sent by Tkinter 
    """

    with open('%s\\%s.json' %(RIGPATH, event.widget.get())) as f:
        profile_dict.update(json.load(f))
    print(profile_dict)

def saveAsEventHandler():
    """
    Allows the user to save a new test config.  This function is called when the user clicks the 'Save As' button.
    """

    modifyJson()
    f = asksaveasfile(initialdir=DATAPATH, initialfile=selected_profile.get()+' - COPY',
                         defaultextension='.json',filetypes=[('JSON Files','*.json')])
    if f is not None:
        json.dump(profile_dict, f, indent=4)


def runTestEventHandler():
    """
    The root of the actual test code.  This function is called when the user clicks the 'Run Test' button.
    """

    global global_serial_object
    global global_run_thread_flag
    global decog_stuck_flag

    # setup serial & readback thread
    if global_serial_object is None or not global_serial_object.isOpen():
        global_serial_object = connect(selected_comport.get().split(' - ')[0], 115200)
    if global_serial_object is None:
        tkMsg.Message(icon=tkMsg.ERROR, message='No test rig is connected.  Please check connection before starting the test.', title='Test Rig Not Connected').show()
        return
    
    decog_stuck_flag = [0]*10
    t1 = threading.Thread(target=getDataThread, args=(global_serial_object,))
    global_run_thread_flag = True
    t1.start()

    # check that user has chosen all the profiles
    if selected_profile.get() == '' or selected_profile.get() == 'SELECT PROFILE':
        tkMsg.Message(icon=tkMsg.ERROR, message='No test profile is selected.  Please select one before starting the test.', title='Test Profile Not Selected').show()
        return
    if selected_test.get() == '' or selected_test.get() == 'SELECT TEST':
        tkMsg.Message(icon=tkMsg.ERROR, message='No test format is selected.  Please select one before starting the test.', title='Test Format Not Selected').show()
        return
    if selected_comport.get() == '' or selected_comport.get() == 'SELECT COM PORT':
        tkMsg.Message(icon=tkMsg.ERROR, message='No COM port is selected.  Please select one before starting the test.', title='COM Port Not Selected').show()
        return
    if selected_rig.get() == '' or selected_rig.get() == 'SELECT RIG':
        tkMsg.Message(icon=tkMsg.ERROR, message='No rig profile is selected.  Please select one before starting the test.', title='Rig Profile Not Selected').show()
        return
    if selected_test.get() == 'STEPPER SPEED' and (selected_step_test.get() == '' or selected_step_test.get() == 'SELECT STEP PROFILE'):
        tkMsg.Message(icon=tkMsg.ERROR, message='No stepper profile is selected.  Please select one before starting the test.', title='Stepper Profile Not Selected').show()
        return

    outlog.delete('1.0', tk.END)
    labelr['text'] = ''

    # send test data to controller to run
    modifyJson()
    if validateJson():
        data = transcribeJson()

        # the endianness of the packet generating the checksum needs to be flipped to match the CRC on the STM32
        # Section 4.3 of the doc linked blow gives details on STM32 CRC32 algorithm
        # https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-415-stm32f407-417-stm32f427-437-and-stm32f429-439-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf
        data_check = bytearray(data)   # would be nice to do this without duplicating the data packet
        swap_endianness(data_check, 4)
        crc_config = crc.Configuration(32, 0x4C11DB7, 0xFFFFFFFF, 0x00000000, False, False) # these are the settings on the STM32 CRC
        checksum = crc.Calculator(crc_config).checksum(data_check)

        global_serial_object.write(len(data).to_bytes(4, 'little'))
        global_serial_object.write(checksum.to_bytes(4, 'little'))
        global_serial_object.read(4)
        global_serial_object.write(data)

        if profile_dict['test']['format'] in ['STEPPER SPEED']:
            sendCsv(global_serial_object)
        elif profile_dict['actuator']['model'] == "CALIBRATION":
            popupCal(global_serial_object)
        

def updateRightUI(event):
    """
    Updates the right side of the UI according to the selected test type.
    Currently, this just enables/disables relevant UI elements.
    """

    if selected_test.get() == 'NONE':
        NumberRunsEntry.config(state='disabled')
        StartLoadEntry.config(state='disabled')
        EndLoadEntry.config(state='disabled')
        step_test_cb.config(state='disabled')
    elif selected_test.get() == 'RAMP':
        NumberRunsEntry.config(state='normal')
        StartLoadEntry.config(state='normal')
        EndLoadEntry.config(state='normal')
        step_test_cb.config(state='disabled')
    elif selected_test.get() == 'STEPPER SPEED':
        NumberRunsEntry.config(state='disabled')
        StartLoadEntry.config(state='disabled')
        EndLoadEntry.config(state='disabled')
        step_test_cb.config(state='normal')
    elif selected_test.get() == 'STEPPER STALL':
        NumberRunsEntry.config(state='disabled')
        StartLoadEntry.config(state='normal')
        EndLoadEntry.config(state='normal')
        step_test_cb.config(state='disabled')
    else:
        pass


# MAIN CODE
if __name__ == '__main__':
    root = tk.Tk()

    #Lists of selections available for comboboxes in UI
    MOTOR_TYPES = ['BRUSHED', 'BRUSHLESS', 'STEPPER']
    ACT_TYPES = ['S']
    REEL_TYPES = ['A', 'B', 'C', 'D', 'E']
    TEST_TYPES = ['RAMP','STEPPER SPEED', 'STEPPER STALL', 'POSITION', 'MAX FORCE', 'NONE']

    #Actuator Characteristics
    modelNumber = tk.StringVar()
    motorType = tk.StringVar()
    strokeLength = tk.StringVar()
    gear = tk.StringVar()
    voltage = tk.StringVar()
    actType = tk.StringVar()

    #Rig Characteristics
    rewindSpeed = tk.StringVar()
    decog = tk.StringVar()
    maglim = tk.StringVar()
    extlim = tk.StringVar()
    retlim = tk.StringVar()
    reel = tk.StringVar()
    hang = tk.StringVar()

    #Test Characteristics
    dutyCycle = tk.StringVar()
    speed = tk.StringVar()
    extend = tk.StringVar()
    retract = tk.StringVar()
    testFormat = tk.StringVar()
    startLoad = tk.StringVar()
    endLoad = tk.StringVar()
    numberRuns = tk.StringVar()
    findMax = tk.StringVar()
    warmup = tk.StringVar()
    stallCurrent = tk.StringVar()
    noLoad = tk.StringVar()
    reducedSpeed = tk.StringVar()       #new addition
    
    #Force Check Characteristics
    #minForce = tk.StringVar()
    #mSpeed = tk.StringVar()
    #cSpeed = tk.StringVar()
    #mCurrent = tk.StringVar()
    #cCurrent = tk.StringVar()
    #speedTolerance = tk.StringVar()
    #currentTolerance = tk.StringVar()

    #Tic Characteristics
    stepAccel = tk.StringVar()
    stepSpeed = tk.StringVar()
    stepCurrent = tk.StringVar()
    stepMode = tk.StringVar()
    stepSize = tk.StringVar()
    stepDecel = tk.StringVar()      #new addition

    # config the root window
    root.geometry('930x450')
    root.resizable(False, False)
    root.title('Performance Test Rig')

    #Initializes tabs, designates their labels, and packs them into the UI
    tabControl = ttk.Notebook(root)
    tab1 = ttk.Frame(tabControl)
    tab2 = ttk.Frame(tabControl)
    tabControl.add(tab1, text="Configuration")
    tabControl.add(tab2, text="Serial Monitor")
    tabControl.pack(expand=1, fill="both")

    #########################################################
    ################# Config Tab ############################

    frameL = tk.Frame(tab1, bg='light grey')
    frameRa = tk.Frame(tab1)
    frameL.pack(anchor=tk.N, fill=tk.BOTH, expand=False, side=tk.LEFT)
    frameRa.pack(anchor=tk.N, fill=tk.BOTH, expand=True, side=tk.RIGHT)

    frameR = tk.Frame(frameRa)
    frameR.pack(anchor=tk.N, fill=tk.BOTH, expand=True, side=tk.TOP,padx=5)

    frameBT = tk.Frame(frameRa, bg='light grey')
    frameBT.pack(anchor=tk.S, fill=tk.X, expand=True, side=tk.BOTTOM)

    # label
    profile_label = ttk.Label(frameL, text='Test Profile:')
    profile_label.configure(background='light grey')
    profile_label.grid(row=0, column=0, columnspan=3, sticky=tk.W+tk.E, padx=GRIDPADX, pady=GRIDPADY)

    ###Profile select combo box
    selected_profile = tk.StringVar()
    selected_profile.set('SELECT PROFILE')
    profile_cb = ttk.Combobox(frameL, textvariable=selected_profile, width=30)
    profile_cb['values'] = [file.removesuffix('.json') for file in os.listdir(DATAPATH) if file.endswith('.json')]
    profile_cb.grid(row=1, column=0,columnspan=2, padx=GRIDPADX, pady=GRIDPADY)
    profile_cb.bind('<<ComboboxSelected>>', loadProfileEventHandler)
    ###End Profile select combo box

    ttk.Separator(frameL).grid(row=2, column=0, columnspan=3, sticky=tk.W+tk.E, pady=5)

    # label
    comport_label = ttk.Label(frameL, text='COM Port:')
    comport_label.configure(background='light grey')
    comport_label.grid(row=3, column=0, columnspan=3, sticky=tk.W+tk.E, padx=GRIDPADX, pady=GRIDPADY)

    ### COM Port select combo box
    comport_list = [(port + ' - ' + desc) for port, desc, hwid in sorted(serial.tools.list_ports.grep('STLink'))]
    selected_comport = tk.StringVar()
    # auto-select a COM port labeled STLink
    selected_comport.set(comport_list[0] if comport_list != [] else 'SELECT COM PORT')

    comport_cb = ttk.Combobox(frameL, textvariable=selected_comport, width=30)
    comport_cb['values'] = comport_list
    comport_cb.grid(row=4, column=0, columnspan=2, padx=GRIDPADX, pady=GRIDPADY)
    ### End COM Port select combo box

    ttk.Separator(frameL).grid(row=5, column=0, columnspan=3, sticky=tk.W+tk.E, pady=5)

    ###Test Config Label
    labelT = ttk.Label(frameL, text='Test Configuration:')
    labelT.configure(background='light grey')
    labelT.grid(row=6, column=0, columnspan=3, sticky=tk.W+tk.E, padx=GRIDPADX, pady=GRIDPADY)

    ###Test type combo box
    selected_test = tk.StringVar()
    test_cb = ttk.Combobox(frameL, textvariable=selected_test, width=30)
    test_cb.set('SELECT TEST')
    test_cb['values'] = TEST_TYPES
    test_cb.grid(row=7, column=0, columnspan=2, padx=GRIDPADX, pady=GRIDPADY)
    test_cb.bind('<<ComboboxSelected>>', updateRightUI)
    ###End test type combo box
    
    ### Stepper speed test profile combo box
    selected_step_test = tk.StringVar()
    selected_step_test.set('SELECT STEP PROFILE')
    step_test_cb = ttk.Combobox(frameL, textvariable=selected_step_test, width=23)
    step_test_cb['values'] = [i for i in os.listdir(DATAPATH) if i.endswith('.csv')]
    step_test_cb.grid(row=8, column=0,columnspan=2, sticky=tk.W+tk.E, padx=GRIDPADX, pady=GRIDPADY)
    ### End stepper speed test profile combo box

    ttk.Separator(frameL).grid(row=9, column=0, columnspan=3, sticky=tk.W+tk.E, pady=5)

    ###Test Config Label
    labelT = ttk.Label(frameL, text='Rig Configuration:')
    labelT.configure(background='light grey')
    labelT.grid(row=10, column=0, columnspan=3, sticky=tk.W+tk.E, padx=GRIDPADX, pady=GRIDPADY)
    ###End config label

    ###Rig config select
    selected_rig = tk.StringVar()
    rig_cb = ttk.Combobox(frameL, textvariable=selected_rig, width=30)
    rig_cb.set('SELECT RIG')
    rig_cb['values'] = [file.removesuffix('.json') for file in os.listdir(RIGPATH) if file.endswith('.json')]
    rig_cb.grid(row=11, column=0, columnspan=2, padx=GRIDPADX, pady=GRIDPADY)
    rig_cb.bind('<<ComboboxSelected>>', loadRigEventHandler)
    ###End rig config 
    
    ttk.Separator(frameL).grid(row=12, column=0, columnspan=3, sticky=tk.W+tk.E, pady=5)

    # label
    #label = ttk.Label(frameL, text='Please select a profile:')
    #label.configure(background='light grey')
    #label.grid(row=8, column=0, columnspan=3, sticky=tk.W+tk.E, padx=GRIDPADX, pady=GRIDPADY)

    ###Hang Extra weight option
    #ew_label = ttk.Label(frameL, text="Hang Extra Weight (Kg):")
    #ew_label.grid(row=2, column=0, padx=GRIDPADX, pady=GRIDPADY)
    #ew_label.configure(background='light grey')

    #hangwt = tk.StringVar()
    #hangwt_ent = ttk.Entry(frameL, textvariable = hangwt,width=3)
    #hangwt_ent.grid(row=2, column=1, padx=GRIDPADX, pady=GRIDPADY)
    ###End hang extra weight option

    ###Reel setting
    re_label = ttk.Label(frameL, text='Use Reel:')
    re_label.grid(row=13, column=0, columnspan=3, sticky=tk.W+tk.E, padx=GRIDPADX, pady=GRIDPADY)
    re_label.configure(background='light grey')

    reel = tk.StringVar()
    reel_ent = ttk.Entry(frameL, textvariable = reel,width=3)
    reel_ent.grid(row=13, column=1, padx=GRIDPADX, pady=GRIDPADY)
    reel_ent.config(state= 'disabled')
    ###End reel setting

    ttk.Separator(frameL).grid(row=14, column=0, columnspan=3, sticky=tk.W+tk.E, pady=5)

    ###detailed output checkbox
    detailOP=tk.StringVar()
    dop_chkb = ttk.Checkbutton(frameL, text='Generate Detailed Output', variable=detailOP, onvalue=1, offvalue=0)
    dop_chkb.grid(row=15, column=0, columnspan=3, sticky=tk.W+tk.E, padx=GRIDPADX)
    detailOP.set('0')
    ###End detail checkbox
    
    ###note entry
    note_label = ttk.Label(frameL, text='Test Note:', background='light grey')
    note_label.grid(row=16, column=0, sticky=tk.W+tk.E, padx=GRIDPADX, pady=GRIDPADY)

    test_note = tk.StringVar()
    test_note_ent = ttk.Entry(frameL, textvariable = test_note, width=32)
    test_note_ent.grid(row=17, column=0, columnspan=3, padx=GRIDPADX, pady=GRIDPADY)
    ###End note entry

    ##########################################################################
    ######################## Config Tab ######################################

    # Actuator
    Field1 = tk.Label(frameR, text="Actuator",padx=1,pady=1)
    Field1.grid(row=0,column=0)
    formatField1 = tk.font.Font(Field1, Field1.cget("font"))
    formatField1.configure(underline = True, weight='bold')
    Field1.configure(font=formatField1)

    modelNumLab = tk.Label(frameR, text="Model #: ", padx=1,pady=3)
    modelNumLab.grid(row=1,column=0)
    modelNumEntry = tk.Entry(frameR, textvariable=modelNumber)
    modelNumEntry.grid(column=1,row=1, ipadx=10)

    MotorLab = tk.Label(frameR, text="Motor: ", padx=1,pady=3, justify='right')
    MotorLab.grid(row=1,column=2)
    MotorDrop = ttk.Combobox(frameR, textvariable=motorType)
    MotorDrop.grid(column=3,row=1)
    MotorDrop['values'] = MOTOR_TYPES

    StrokeLenLab = tk.Label(frameR, text="Stroke Length: ", padx=1,pady=3, justify='right')
    StrokeLenLab.grid(column=4,row=1)
    StrokeLenEntry = tk.Entry(frameR, textvariable=strokeLength)
    StrokeLenEntry.grid(column=5,row=1, ipadx=10)

    VoltageLab = tk.Label(frameR, text="Voltage: ", padx=1,pady=3, justify='right')
    VoltageLab.grid(column=0,row=2)
    VoltageEntry = tk.Entry(frameR, textvariable=voltage)
    VoltageEntry.grid(column=1,row=2, ipadx=10)

    GearLab = tk.Label(frameR, text="Gear: ", padx=1,pady=3, justify='right')
    GearLab.grid(column=2,row=2)
    GearEntry = tk.Entry(frameR, textvariable=gear)
    GearEntry.grid(column=3,row=2, ipadx=10)

    TypeLab = tk.Label(frameR, text="Type: ", padx=1,pady=3, justify='right')
    TypeLab.grid(column=4,row=2)
    Type = ttk.Combobox(frameR, textvariable=actType)
    Type.grid(column=3,row=1)
    Type['values'] = ACT_TYPES
    Type.grid(column=5,row=2)

    # Rig
    Field2 = tk.Label(frameR, text="Rig",padx=1,pady=1,justify='left')
    Field2.grid(row=3,column=0)
    formatField2 = tk.font.Font(Field2, Field2.cget("font"))
    formatField2.configure(underline = True, weight='bold')
    Field2.configure(font=formatField2)

    RewindLab = tk.Label(frameR, text="Rewind Spd: ", padx=1,pady=3, justify='right', )
    RewindLab.grid(column=0,row=4)
    RewindEntry = tk.Entry(frameR, textvariable=rewindSpeed)
    RewindEntry.grid(column=1,row=4, ipadx=10)    

    DecogLab = tk.Label(frameR, text="Decog: ", padx=1,pady=3, justify='right')
    DecogLab.grid(column=2,row=4)
    DecogEntry = tk.Entry(frameR, textvariable=decog)
    DecogEntry.grid(column=3,row=4, ipadx=10)     

    MagLimLab = tk.Label(frameR, text="Mag Lim: ", padx=1,pady=3, justify='right')
    MagLimLab.grid(column=4,row=4)
    MagLimEntry = tk.Entry(frameR, textvariable=maglim)
    MagLimEntry.grid(column=5,row=4, ipadx=10)     

    ExtLimLab = tk.Label(frameR, text="Ext Lim: ", padx=1,pady=3, justify='right')
    ExtLimLab.grid(column=0,row=5)
    ExtLimEntry = tk.Entry(frameR, textvariable=extlim)
    ExtLimEntry.grid(column=1,row=5, ipadx=10)

    RetLimLab = tk.Label(frameR, text="Ret Lim: ", padx=1,pady=3, justify='right')
    RetLimLab.grid(column=2,row=5)
    RetLimEntry = tk.Entry(frameR, textvariable=retlim)
    RetLimEntry.grid(column=3,row=5, ipadx=10)

    ReelLab = tk.Label(frameR, text="Reel: ", padx=1,pady=3, justify='right')
    ReelLab.grid(column=4,row=5)
    ReelBox = ttk.Combobox(frameR, textvariable=reel)
    ReelBox.grid(column=5,row=5)
    ReelBox['values'] = REEL_TYPES

    HangWtLab = tk.Label(frameR, text="Hang: ", padx=1,pady=3, justify='right')
    HangWtLab.grid(column=0,row=6)
    HangWtEntry = tk.Entry(frameR, textvariable=hang)
    HangWtEntry.grid(column=1,row=6, ipadx=10)

    # Test
    Field3 = tk.Label(frameR, text="Test",padx=1,pady=1)
    Field3.grid(row=7,column=0)
    formatField3 = tk.font.Font(Field3, Field3.cget("font"))
    formatField3.configure(underline = True, weight='bold')
    Field3.configure(font=formatField3)

    DutyLab = tk.Label(frameR, text="Duty: ", padx=1,pady=3, justify='right')
    DutyLab.grid(column=0,row=8)
    DutyEntry = tk.Entry(frameR, textvariable=dutyCycle)
    DutyEntry.grid(column=1,row=8, ipadx=10)

    SpeedLab = tk.Label(frameR, text="Speed: ", padx=1,pady=3, justify='right')
    SpeedLab.grid(column=2,row=8)
    SpeedEntry = tk.Entry(frameR, textvariable=speed)
    SpeedEntry.grid(column=3,row=8, ipadx=10)

    """New addition below"""
    ReducedSpeedLab = tk.Label(frameR, text="Speed Reduction: ", padx=1,pady=3, justify='right')
    ReducedSpeedLab.grid(column=4,row=8)
    ReducedSpeedEntry = tk.Entry(frameR, textvariable=reducedSpeed)
    ReducedSpeedEntry.grid(column=5,row=8, ipadx=10)
    """End new addition"""

    ExtendLab = tk.Label(frameR, text="Extend: ", padx=1,pady=3, justify='right')
    ExtendLab.grid(column=0,row=9)
    ExtendEntry = tk.Entry(frameR, textvariable=extend)
    ExtendEntry.grid(column=1,row=9, ipadx=10)

    RetractLab = tk.Label(frameR, text="Retract: ", padx=1,pady=3, justify='right')
    RetractLab.grid(column=2,row=9)
    RetractEntry = tk.Entry(frameR, textvariable=retract)
    RetractEntry.grid(column=3,row=9, ipadx=10)

    #FormatLab = tk.Label(frameR, text="Format: ", padx=1,pady=3, justify='right')
    #FormatLab.grid(column=4,row=9)
    #FormatBox = ttk.Combobox(frameR, textvariable=testFormat)
    #FormatBox.grid(column=5,row=9)
    #FormatBox['values'] = TEST_TYPES
    #FormatBox.bind('<<ComboboxSelected>>', updateRightUI)

    NumberRunsLab = tk.Label(frameR, text="Runs: ", padx=1,pady=3, justify='right')
    NumberRunsLab.grid(column=0,row=10)
    NumberRunsEntry = tk.Entry(frameR, textvariable=numberRuns)
    NumberRunsEntry.grid(column=1,row=10, ipadx=10)

    StartLoadLab = tk.Label(frameR, text="Start Load: ", padx=1,pady=3, justify='right')
    StartLoadLab.grid(column=2,row=10)
    StartLoadEntry = tk.Entry(frameR, textvariable=startLoad)
    StartLoadEntry.grid(column=3,row=10, ipadx=10)

    EndLoadLab = tk.Label(frameR, text="End Load: ", padx=1,pady=3, justify='right')
    EndLoadLab.grid(column=4,row=10)
    EndLoadEntry = tk.Entry(frameR, textvariable=endLoad)
    EndLoadEntry.grid(column=5,row=10, ipadx=10)   

    #minForceLab = tk.Label(frameR, text="minForce: ", padx=1,pady=3, justify='right')
    #minForceLab.grid(column=0,row=12)
    #minForceEntry = tk.Entry(frameR, textvariable=minForce)
    #minForceEntry.grid(column=1,row=12, ipadx=10)

    #mSpeedLab = tk.Label(frameR, text="mSpeed: ", padx=1,pady=3, justify='right')
    #mSpeedLab.grid(column=2,row=12)
    #mSpeedEntry = tk.Entry(frameR, textvariable=mSpeed)
    #mSpeedEntry.grid(column=3,row=12, ipadx=10)

    #cSpeedLab = tk.Label(frameR, text="cSpeed: ", padx=1,pady=3, justify='right')
    #cSpeedLab.grid(column=0,row=13)
    #cSpeedEntry = tk.Entry(frameR, textvariable=cSpeed)
    #cSpeedEntry.grid(column=1,row=13, ipadx=10)

    #mCurrentLab = tk.Label(frameR, text="mCurrent: ", padx=1,pady=3, justify='right')
    #mCurrentLab.grid(column=2,row=13)
    #mCurrentEntry = tk.Entry(frameR, textvariable=mCurrent)
    #mCurrentEntry.grid(column=3,row=13, ipadx=10)

    #cCurrentLab = tk.Label(frameR, text="cCurrent: ", padx=1,pady=3, justify='right')
    #cCurrentLab.grid(column=4,row=13)
    #cCurrentEntry = tk.Entry(frameR, textvariable=cCurrent)
    #cCurrentEntry.grid(column=5,row=13, ipadx=10)

    #speedToleranceLab = tk.Label(frameR, text="speedTolerance: ", padx=1,pady=3, justify='right')
    #speedToleranceLab.grid(column=0,row=14)
    #speedToleranceEntry = tk.Entry(frameR, textvariable=speedTolerance)
    #speedToleranceEntry.grid(column=1,row=14, ipadx=10)

    #currentToleranceLab = tk.Label(frameR, text="currentTolerance: ", padx=1,pady=3, justify='right')
    #currentToleranceLab.grid(column=2,row=14)
    #currentToleranceEntry = tk.Entry(frameR, textvariable=currentTolerance)
    #currentToleranceEntry.grid(column=3,row=14, ipadx=10)   
 
    # Tic
    Field4 = tk.Label(frameR, text="TIC",padx=1,pady=1)
    Field4.grid(row=15,column=0)
    formatField4 = tk.font.Font(Field4, Field4.cget("font"))
    formatField4.configure(underline = True, weight='bold')
    Field4.configure(font=formatField4)

    maxAccelLab = tk.Label(frameR, text="Max Accel: ", padx=1,pady=3, justify='right')
    maxAccelLab.grid(column=0,row=16)
    maxAccelEntry = tk.Entry(frameR, textvariable=stepAccel)
    maxAccelEntry.grid(column=1,row=16, ipadx=10)

    """New addition below"""
    maxDecelLab = tk.Label(frameR, text="Max Decel: ", padx=1,pady=3, justify='right')
    maxDecelLab.grid(column=4,row=16)
    maxDecelEntry = tk.Entry(frameR, textvariable=stepDecel)
    maxDecelEntry.grid(column=5,row=16, ipadx=10)
    """End new addition """

    #change below to columns 4 and 5 once deceleration is implemented
    startSpeedLab = tk.Label(frameR, text="Start Speed: ", padx=1,pady=3, justify='right')
    startSpeedLab.grid(column=2,row=16)
    startSpeedEntry = tk.Entry(frameR, textvariable=stepSpeed)
    startSpeedEntry.grid(column=3,row=16, ipadx=10)

    maxCurrentLab = tk.Label(frameR, text="Max Current: ", padx=1,pady=3, justify='right')
    maxCurrentLab.grid(column=0,row=17)
    maxCurrentEntry = tk.Entry(frameR, textvariable=stepCurrent)
    maxCurrentEntry.grid(column=1,row=17, ipadx=10)

    stepModeLab = tk.Label(frameR, text="Step Mode: ", padx=1,pady=3, justify='right')
    stepModeLab.grid(column=2,row=17)
    stepModeEntry = tk.Entry(frameR, textvariable=stepMode)
    stepModeEntry.grid(column=3,row=17, ipadx=10)

    stepSizeLab = tk.Label(frameR, text="Step Size: ", padx=1,pady=3, justify='right')
    stepSizeLab.grid(column=4,row=17)
    stepSizeEntry = tk.Entry(frameR, textvariable=stepSize)
    stepSizeEntry.grid(column=5,row=17, ipadx=10)

    ################################################### 

    ###SAVEAS button
    saveas = tk.Button(frameBT, text='Save As', command=saveAsEventHandler)
    saveas.pack(anchor=tk.N, fill=tk.BOTH, expand=True, side=tk.LEFT)
    ###END SAVEAS

    ###RUN button
    run = tk.Button(frameBT, text='Run Test', command=runTestEventHandler)
    run.pack(anchor=tk.N, fill=tk.BOTH, expand=True, side=tk.RIGHT)
    ###END RUN

    # start with options disabled (until relevant test is selected)
    #runs_ent.config(state='disabled')
    #init_load_ent.config(state='disabled')
    #end_load_ent.config(state='disabled')
    #step_test_cb.config(state='disabled')

    ##########################################################################
    ############################# Serial Tab #################################
    # Output Text
    frameBT1 = tk.Frame(tab2, bg='white')
    frameBT1.pack(anchor=tk.N, fill=tk.X, expand=True, side=tk.TOP)
    # label
    labelr = ttk.Label(frameBT1, text=" ",width= 6, wraplength= 48)
    labelr.configure(background='light grey')
    labelr.config(font=('Helvatical bold',16))
    labelr.pack(anchor=tk.E, fill=tk.BOTH, expand=True, side=tk.RIGHT)
    outlog = tk.Text(frameBT1, height='24')
    outlog.pack(anchor=tk.W, fill=tk.Y, expand=True, side=tk.LEFT)

    ##########################################################################

    root.mainloop()