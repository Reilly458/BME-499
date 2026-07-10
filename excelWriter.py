import pandas as pd
import xlsxwriter
import numpy as np
from scipy import stats
import os
import time

from csvAnalytics import stepperSpeedProcessor
#from TestRig_Release import decog_stuck_flag

decog_stuck_flag = [0] * 10 # added by Reilly to indicate stall in decog phase

#this class describes an excel spreadsheet document
class excelDoc:

    def __init__(self, file):
        self.writer = pd.ExcelWriter(file, engine='xlsxwriter')
        self.fname = file

    def saveDoc(self):
        self.writer.close()
        os.startfile(self.fname) #open file


#This class describes a sheet within an excel document
class fillSheet:

    def __init__(self,doc,sheet): #(document obj., sheet name)
        #compies properties from parent document
        self.writer = doc.writer
        self.sheet = sheet
        self.fname = doc.fname
        self.workbook = self.writer.book
        #pre-made formatting definitions (could not put these at top of class as attributes because workbook had to be initialized)
        self.format_pass = self.workbook.add_format({'bold': True ,'bg_color': '#339966','font_color': '#000000'})
        self.format_fail = self.workbook.add_format({'bold': True ,'bg_color': '#FF8080','font_color': '#000000'})
        self.format_bold = self.workbook.add_format({'bold': True})
        self.percent_format_pass = self.workbook.add_format({'num_format': '0.00%','bg_color': '#339966','font_color': '#000000'})
        self.percent_format_fail = self.workbook.add_format({'num_format': '0.00%','bg_color': '#FF8080','font_color': '#000000'})
        self.top_border = self.workbook.add_format({'border':2})

    #generic function which inserts dataframe which specified precision starting at row and column
    def insertData(self,data,precision,row,col):
        data.round(precision).to_excel(self.writer,
                                       sheet_name=self.sheet,
                                       startrow=row,
                                       startcol=col,
                                       index= False)

    #apply general formatting, intend to fill this with more later
    def generalFormat(self):
        self.set_column_width()

    #auto-formats column width for main dataframe
    def set_column_width(self):
        length_list = [len(x) for x in self.data.columns]
        for i, width in enumerate(length_list):
            self.worksheet.set_column(i, i, width)

    #close the current workbook to remove any handles
    def close(self):
        self.workbook.close()


#inherited class of fillSheet, specific to ramp test results
class rampSheet(fillSheet):

    sheet = 'Ramp Test' #sheet name

    def __init__(self,doc,data,specs,checks,note): #(document obj., test dataframe, standards dataframe, pass/fail checks, user note)
        super(rampSheet,self).__init__(doc,self.sheet)
        self.data = data
        self.note = note
        self.specs = specs
        self.checks = checks
        self.insertData(data,3,0,0)
        self.insertData(specs, 3,0,14)
        self.worksheet = self.writer.sheets[self.sheet]
        self.rampChart()
        self.results()
        self.rampFormat()
        self.generalFormat()

    #create the ramp sheet charts
    def rampChart(self):
        chart = self.workbook.add_chart({'type': 'scatter',
                             'subtype': 'straight'})
        # Configure the series of the chart from the dataframe data.
        chart.add_series({
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 1, len(self.data), 1]
        })
        chart.add_series({
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 18, len(self.data), 18],
            'line':       {'color': 'red','dash_type': 'round_dot'},
        })
        chart.add_series({
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 19, len(self.data), 19],
            'line':       {'color': 'red','dash_type': 'round_dot'},
        })
        # Configure the chart axes.
        chart.set_x_axis({'name': 'Force (N)', 'position_axis': 'on_tick'})
        chart.set_y_axis({'name': 'Current (mA)', 'major_gridlines': {'visible': True}})
        chart.set_legend({'position': 'none'})
        chart.set_title({ 'name': 'Current vs Force'})
        chart2 = self.workbook.add_chart({'type': 'scatter',
                                 'subtype': 'straight'})
        # Configure the series of the chart from the dataframe data.
        chart2.add_series({
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 2, len(self.data), 2]
        })
        chart2.add_series({
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 15, len(self.data), 15],
            'line':       {'color': 'red','dash_type': 'round_dot'},
        })
        chart2.add_series({
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 16, len(self.data), 16],
            'line':       {'color': 'red','dash_type': 'round_dot'},
        })
        # Configure the chart axes.
        chart2.set_x_axis({'name': 'Force (N)', 'position_axis': 'on_tick'})
        chart2.set_y_axis({'name': 'Speed (mm/s)', 'major_gridlines': {'visible': True}})
        chart2.set_legend({'position': 'none'})
        chart2.set_title({ 'name': 'Speed vs Force'})
        self.worksheet.insert_chart('G4', chart)
        self.worksheet.insert_chart('G19', chart2)

    #insert results data and formatting to the sheet
    def results(self):
        ypos = len(self.data)
        #pass/fail checks
        self.worksheet.write(ypos+4, 3, 'FORCE CHECK', self.format_bold)
        self.worksheet.write(ypos+4, 4, self.checks[0])
        self.worksheet.write(ypos+5, 3, 'CURRENT CHECK', self.format_bold)
        self.worksheet.write(ypos+5, 4, self.checks[1])
        self.worksheet.write(ypos+6, 3, 'SPEED CHECK', self.format_bold)
        self.worksheet.write(ypos+6, 4, self.checks[2])
        #max values
        self.worksheet.write(ypos+8, 3, 'Max Force (N)', self.format_bold)
        self.worksheet.write(ypos+8, 4, self.checks[3])
        self.worksheet.write(ypos+9, 3, 'Current @ Max Force (mA)', self.format_bold)
        self.worksheet.write(ypos+9, 4, self.checks[4])
        self.worksheet.write(ypos+10, 3, 'Speed @ Max Force (mm/s)', self.format_bold)
        self.worksheet.write(ypos+10, 4, self.checks[5])
        self.worksheet.write(ypos+11, 3, 'Est. NL Speed (mm/s)', self.format_bold)
        self.worksheet.write(ypos+11, 4, self.checks[6])
        #User Note
        self.worksheet.write(1, 7, self.note)

    #apply conditional formatting to the results
    def rampFormat(self):
        self.worksheet.conditional_format(len(self.data)+4,4,len(self.data)+6,4,{'type':     'text',
                                                    'criteria': 'containing',
                                                    'value':    'FAIL',
                                                    'format':   self.format_fail})
        self.worksheet.conditional_format(len(self.data)+4,4,len(self.data)+6,4,{'type':     'text',
                                                    'criteria': 'containing',
                                                    'value':    'PASS',
                                                    'format':   self.format_pass})


#inherited class of fillSheet, gives detailed breakdown of individual run data
class runSheet(fillSheet):

    def __init__(self,doc,data,trim,run):
        self.sheet = 'Run ' + str(run) #sheet name
        super(runSheet,self).__init__(doc,self.sheet) #init super class
        self.data = data
        self.trim = trim
        self.insertData(data, 4, 0, 0)
        self.worksheet = self.writer.sheets[self.sheet]
        self.runChart()
        self.generalFormat()

    def runChart(self):

        self.dataRange() #insert data for the inclusion bars
        self.configChart() #set up the chart

    def dataRange(self):
        #generate vertical lines on chart that indicates data range
        self.worksheet.write(0, 23, 'Time Range')
        self.worksheet.write(0, 24, 'Force')
        self.worksheet.write(1, 23, self.trim[0])
        self.worksheet.write(1, 24, self.trim[1])
        self.worksheet.write(2, 23, self.trim[0])
        self.worksheet.write(2, 24, 0)
        self.worksheet.write(3, 23, self.trim[2])
        self.worksheet.write(3, 24, self.trim[3])
        self.worksheet.write(4, 23, self.trim[2])
        self.worksheet.write(4, 24, 0)

    def configChart(self):
        chart = self.workbook.add_chart({'type': 'scatter','subtype': 'straight'})

        # Configure the series of the chart from the dataframe data.
        chart.add_series({
            'name': 'Measured Force (N)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 5, len(self.data), 5],
            'line':       {'color': 'blue'},
            'y2_axis': 1,
        })
        chart.add_series({
            'name': 'Measured Position (mm)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 4, len(self.data), 4],
            'line':       {'color': 'green'},
        })
        chart.add_series({
            'name': 'Measured Current (mA)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 6, len(self.data), 6],
            'line':       {'color': 'red'},
        })
        chart.add_series({
            'name': 'Target Force (N)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values':     [self.sheet, 1, 2, len(self.data), 2],
            'y2_axis': 1,
            'line':       {'color': 'blue', 'dash_type': 'round_dot'},
        })
        chart.add_series({
            'name': 'Data Range Start',
            'categories': [self.sheet, 1, 23, 2, 23],
            'values':     [self.sheet, 1, 24, 2, 24],
            'y2_axis': 1,
            'line':       {'color': 'orange', 'dash_type': 'dash', 'width':2},
        })
        chart.add_series({
            'name': 'Data Range End',
            'categories': [self.sheet, 3, 23, 4, 23],
            'values':     [self.sheet, 3, 24, 4, 24],
            'y2_axis': 1,
            'line':       {'color': 'orange', 'dash_type': 'dash', 'width':2},
        })

        # Configure the chart axes.
        chart.set_x_axis({'name': 'Time (s)', 'position_axis': 'on_tick'})
        chart.set_y_axis({'name': 'current & position', 'major_gridlines': {'visible': True}, 'min': 0})
        chart.set_y2_axis({'name': 'Force (N)', 'min': 0})

        #chart.set_legend({'position': 'none'})
        chart.set_title({'name': self.sheet})
        chart.set_size({'width': 960, 'height': 576})
        self.worksheet.insert_chart('H4', chart)


#inherited class of fillSheet, generates report of calibration data
class calCheckSheet(fillSheet):

    ACCURACY = 0.02 #Pass/fail threshold on calibration report
    sheet = 'Calibration Report' #sheet name

    def __init__(self,doc,data,CURRENT_CHECK,FORCE_CHECK):
        super(calCheckSheet,self).__init__(doc,self.sheet)
        self.data = data
        self.curCheck = CURRENT_CHECK
        self.forCheck = FORCE_CHECK
        self.insertData(data,5,2,0)
        self.worksheet = self.writer.sheets[self.sheet]
        self.results()
        self.manualFields()
        self.calFormat()
        self.generalFormat()

    #pass/fail results
    def results(self):
        self.worksheet.write(9, 1, 'Current: ', self.format_bold)
        self.worksheet.write(10, 1, 'Force: ', self.format_bold)
        self.worksheet.write(11, 1, 'Voltage: ', self.format_bold)
        self.worksheet.write(9, 2, self.curCheck)#, self.format_bold)
        self.worksheet.write(10, 2, self.forCheck)#, self.format_bold)

    #manual fields for voltage measurement and condition checks
    def manualFields(self):
        self.worksheet.set_column('I:K', 12)
        self.worksheet.merge_range('I2:K2', 'Manual Voltage Check', self.format_bold)
        self.worksheet.merge_range('I3:K3', '(Enter Observed Values Below)')
        self.worksheet.write(3, 8, 'Expected (V)', self.format_bold)
        self.worksheet.write(3, 9, 'Observed (V)', self.format_bold)
        self.worksheet.write(3, 10, '% Error', self.format_bold)
        self.worksheet.write(4, 8, 12)
        self.worksheet.write(5, 8, 6)
        self.worksheet.write_formula('K5', '=(abs(J5)-I5)/I5')
        self.worksheet.write_formula('K6', '=(abs(J6)-I6)/I6')
        self.worksheet.write_formula('C12', '=IF(AND(ABS(K5)<=0.02,ABS(K6)<=0.02),"PASS","FAIL")')

    #formatting for calibration sheet
    def calFormat(self):
        self.worksheet.write(0, 0, 'Performance Test Rig - Routine Calibration Check')
        self.worksheet.write(1, 0, time.strftime('%Y-%m-%d %H:%M:%S'))
        self.worksheet.conditional_format('C4:C6', {'type': 'cell',
                                                    'criteria': 'not between',
                                                    'minimum': -self.ACCURACY,
                                                    'maximum': self.ACCURACY,
                                                    'format': self.percent_format_fail})
        self.worksheet.conditional_format('C4:C6', {'type': 'cell',
                                                    'criteria': 'between',
                                                    'minimum': -self.ACCURACY,
                                                    'maximum': self.ACCURACY,
                                                    'format': self.percent_format_pass})
        self.worksheet.conditional_format('F4:F6', {'type': 'cell',
                                                    'criteria': 'not between',
                                                    'minimum': -self.ACCURACY,
                                                    'maximum': self.ACCURACY,
                                                    'format': self.percent_format_fail})
        self.worksheet.conditional_format('F4:F6', {'type': 'cell',
                                                    'criteria': 'between',
                                                    'minimum': -self.ACCURACY,
                                                    'maximum': self.ACCURACY,
                                                    'format': self.percent_format_pass})
        self.worksheet.conditional_format('K5:K6', {'type': 'cell',
                                                    'criteria': 'not between',
                                                    'minimum': -self.ACCURACY,
                                                    'maximum': self.ACCURACY,
                                                    'format': self.percent_format_fail})
        self.worksheet.conditional_format('K5:K6', {'type': 'cell',
                                                    'criteria': 'between',
                                                    'minimum': -self.ACCURACY,
                                                    'maximum': self.ACCURACY,
                                                    'format': self.percent_format_pass})
        self.worksheet.conditional_format('C10:C12', {'type': 'text',
                                                    'criteria': 'containing',
                                                    'value': 'FAIL',
                                                    'format': self.format_fail})
        self.worksheet.conditional_format('C10:C12', {'type': 'text',
                                                    'criteria': 'containing',
                                                    'value': 'PASS',
                                                    'format': self.format_pass})

class stepperSpeedSheet(fillSheet):
    sheet = 'Stepper Speed Test' #sheet name

    def __init__(self, doc, data, dataAvg, note, decog_stuck_flag):
        super(stepperSpeedSheet,self).__init__(doc, self.sheet)
        self.data = data
        self.dataAvg = dataAvg
        self.note = note
        self.insertData(pd.DataFrame(range(1, dataAvg.shape[0] + 1), columns=['Run']), 1, 0, 0)
        self.insertData(dataAvg, 3, 0, 1)
        self.worksheet = self.writer.sheets[self.sheet]
        self.worksheet.write(0, 11, 'Note', self.format_bold)
        self.worksheet.write(1, 11, self.note)
        for row in range(dataAvg.shape[0]):
            ### Reilly addition for stuck in decog detection ###
            if decog_stuck_flag[row] == True:
                self.format_pass = self.workbook.add_format({'bold': True ,'bg_color': "#F0D508",'font_color': '#000000'})  # indicates actuator got stuck/stalled in decog phase
            else: self.format_pass = self.workbook.add_format({'bold': True ,'bg_color': '#339966','font_color': '#000000'})
            ### End Reilly addition ###
            self.worksheet.write_formula(f'J{row + 2}', f'=IF(I{row + 2} = 0, "PASS", "FAIL")')
            self.worksheet.conditional_format(f'J{row + 2}', {'type': 'text',
                                                'criteria': 'containing',
                                                'value':    'PASS',
                                                'format':   self.format_pass})
            self.worksheet.conditional_format(f'J{row + 2}', {'type': 'text',
                                                'criteria': 'containing',
                                                'value':    'FAIL',
                                                'format':   self.format_fail})      
        self.generalFormat()

class stepperRunSheet(fillSheet):
    def __init__(self, doc, data, trim):
        self.data = data
        self.sheet = 'Run %d' %(self.data['run'].iat[0]) #sheet name
        self.trim = trim
        super(stepperRunSheet, self).__init__(doc, self.sheet) #init super class
        '''-----------------------------------------------------------'''
        #Reilly addition: takes a rolling average of the measured speed and adds it to the run dataframe
        #can play with window size to see if signal smooths more with larger window or right aligned window
        roll_speed = data['speed'].rolling(5, 1, center = True).mean() 
        self.data['rolling speed avg'] = roll_speed
        '''-----------------------------------------------------------'''
        self.insertData(self.data, 3, 0, 0)
        self.worksheet = self.writer.sheets[self.sheet]
        self.generalFormat()

        # add threshold values
        self.worksheet.write(32, 12, 'Time Range', self.format_bold)
        self.worksheet.write(32, 13, 'Force Threshold', self.format_bold)
        self.worksheet.write(33, 12, self.trim[0])  #Data Range Start Time
        self.worksheet.write(33, 13, self.trim[1])  #Data Range Start Force
        self.worksheet.write(34, 12, self.trim[0])  #Data Range Start Time
        self.worksheet.write(34, 13, 0)
        self.worksheet.write(35, 12, self.trim[2])  #Data Range End Time
        self.worksheet.write(35, 13, self.trim[3])  #Data Range End Force
        self.worksheet.write(36, 12, self.trim[2])  #Data Range End Time
        self.worksheet.write(36, 13, 0)

        
        

        chart = self.workbook.add_chart({'type': 'scatter','subtype': 'straight'})

        # Configure the series of the chart from the dataframe data.
        chart.add_series({
            'name': 'Measured Force (N)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values': [self.sheet, 1, 6, len(self.data), 6],
            'line': {'color': 'blue'},
            'y2_axis': 1,
        })
        chart.add_series({
            'name': 'Target Force (N)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values': [self.sheet, 1, 4, len(self.data), 4],
            'y2_axis': 1,
            'line': {'color': 'blue','dash_type': 'round_dot'},
        })
        chart.add_series({
            'name': 'Target Speed (mm/s)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values': [self.sheet, 1, 3, len(self.data), 3],
            'line': {'color': 'green', 'dash_type': 'round_dot'},
            'y-axis': 1
        })
        
        chart.add_series({
            'name': 'Measured Speed (mm/s)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values': [self.sheet, 1, 10, len(self.data), 10],
            'line': {'color': 'green', 'transparency': 83},
            'y-axis': 1
        })
        chart.add_series({
            'name': 'Measured Current (mA)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values': [self.sheet, 1, 7, len(self.data), 7],
            'line': {'color': 'red'},
        })
        chart.add_series({
            'name': 'Data Range Start',
            'categories': [self.sheet, 33, 12, 34, 12],
            'values': [self.sheet, 33, 13, 34, 13],
            'y2_axis': 1,
            'line': {'color': 'orange','dash_type': 'dash','width':2},
        })
        chart.add_series({
            'name': 'Data Range End',
            'categories': [self.sheet, 35, 12, 36, 12],
            'values': [self.sheet, 35, 13, 36, 13],
            'y2_axis': 1,
            'line': {'color': 'orange','dash_type': 'dash','width':2},
        })

        '''-----------------------------------------------------------
        ---------------------Reilly Addition--------------------------
        -----------------------------------------------------------'''
        
        #Adds rolling averaged measured speed to graph
        chart.add_series({
            'name': 'Rolling Avg Measured Speed (mm/s)',
            'categories': [self.sheet, 1, 0, len(self.data), 0],
            'values': [self.sheet, 1, 11, len(self.data), 11],
            'y_axis': 1,
            'line': {'color' : 'green'},
        })
        '''-----------------------------------------------------------
        ---------------------Reilly Addition--------------------------
        -----------------------------------------------------------'''

        # Configure the chart axes.
        chart.set_x_axis({'name': 'Time (s)', 'position_axis': 'on_tick', 'min': min(self.data['time']), 'max': max(self.data['time'])})
        chart.set_y_axis({'name': 'Speed (mm/s) & Current (mA)', 'major_gridlines': {'visible': True}, 'min': 0})
        chart.set_y2_axis({'name': 'Force (N)', 'min': 0})

        chart.set_title({'name': self.sheet})
        chart.set_size({'width': 960, 'height': 576})
        self.worksheet.insert_chart('M2', chart)

# test code
if __name__ == '__main__':
    doc = excelDoc('test.xlsx')
    foo = stepperSpeedProcessor('stepper_speed_data.csv')
    stepperSpeedSheet(doc, foo.df, foo.dfAvg, 'hello')
    [stepperRunSheet(doc, foo.dfRuns[n], foo.avgTruncList[n]) for n in range(foo.dfAvg.shape[0])]
    doc.saveDoc()