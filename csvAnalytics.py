import pandas as pd
import xlsxwriter
import numpy as np
from scipy import stats
import os

class csvProcessor:

    #attributes

    def __init__(self, fname, complete_dict):
        self.complete_dict = complete_dict
        self.dfRAW = self.removeOutliers(pd.read_csv(fname), 0.02) #Raw data from csv file, with outliers removed (There is an undetermined FW or HW bug causing erratic values occasionally).
        self.df = self.processRaw(self.dfRAW) #Applies calculations to loadcell and position values to get base units.
        self.dfRuns = self.splitRuns(self.df) #Separates the dataframe into n dataframes of each test run, based on the 'run' column.
        self.dfAvg = self.runAvg(0.16)  #returns the avg value (under load) for each run.  Reilly change: originally 0.1, 
        if self.dfAvg.empty:
            raise RuntimeError("Error: Could not perform averaging on CSV data.  This could be because there are too few data points.")

    def printDF(self): #only used for testing and debugging
        print(self.df)

    #start time column at zero by subtracting the first time entry from all rows
    def zeroTime(self, df_in):
        df_in['time'] -= df_in['time'].iat[0]

        return df_in

    def forceRaw(self, loadcell):
        if loadcell > self.complete_dict['loadcell']['threshold']:
            a = self.complete_dict['loadcell']['A_high']
            b = self.complete_dict['loadcell']['B_high']
            c = self.complete_dict['loadcell']['C_high']
            d = self.complete_dict['loadcell']['D_high']
            e = self.complete_dict['loadcell']['E_high']
        else:
            a = self.complete_dict['loadcell']['A_low']
            b = self.complete_dict['loadcell']['B_low']
            c = self.complete_dict['loadcell']['C_low']
            d = self.complete_dict['loadcell']['D_low']
            e = self.complete_dict['loadcell']['E_low']

        return a*pow(loadcell,4) + b*pow(loadcell,3) + c*pow(loadcell,2) + d*loadcell + e

    def processRaw(self, df_in):
        df_in = self.zeroTime(df_in)    #tare the time column to 0ms
        df_in['time'] /= 1000           #convert ms to s
        df_in['force'] = df_in['force'].apply(self.forceRaw) #loadcell equation
        df_in['position'] /= self.complete_dict['posSensor']['scale']      #convert to mm from raw adc val
        return df_in

    #splits entire dataframe into a list of individual test runs, this can likely be done more efficiently with a different Pandas method, such as 'groupby', but is working for now.
    def splitRuns(self,df_in):
        Runs = [] #List of each dataframes containing each run
        #print(df_in)
        for i in range(df_in['run'].iat[-1]): #iterate through 'run' column
            dfRun = df_in[df_in['run'] == i+1].copy()   # copy gets rid of Pandas SettingWithCopyWarning - 
                                                        # dfRun is a local var that may otherwise passed to zeroTime as a view (I think)
            dfRun = self.zeroTime(dfRun)
            dfRun.reset_index(inplace=True,drop=True) #reset indexes
            Runs.append(dfRun)
        return Runs #returns list of dataframes of each run

    def removeOutliers(self, df_in, percent): #removes outliers from position and force columns as percentile for each run
        dfNew = pd.DataFrame([], columns= list(df_in))
        
        # check for empty array that causes IndexError to be thrown in loop below
        if len(df_in['run']) == 0:
            return dfNew
        
        for i in range(df_in['run'].iat[-1]):
            dfRun = df_in[df_in['run'] == i + 1]
            dfRun = self.remove_outlier(dfRun, 'position', percent, percent)
            dfRun = self.remove_outlier(dfRun, 'force', percent, percent)
            dfNew = pd.concat([dfNew, dfRun], ignore_index=True)
        
        return dfNew

    def remove_outlier(self, df_in, col_name, percentH, percentL): #removes rows of dataframe where specified column values are not within percentile range
        q1 = df_in[col_name].quantile(percentL)
        q3 = df_in[col_name].quantile(1 - percentH)
        iqr = q3-q1 #Interquartile range
        fence_low  = q1 - 1.5 * iqr
        fence_high = q3 + 1.5 * iqr
        df_out = df_in[(df_in[col_name] > fence_low) & (df_in[col_name] < fence_high)]
        return df_out

    def runAvg(self, fPercent): #takes a truncated average of a test run dataframe. Data starts at first instance of force within 100-fPercent of maximum force in the run
        avgList = []
        self.avgTruncList = []
        for run in self.dfRuns:
            '''-----------------------------------------------------------
            ---------------------Reilly Addition--------------------------
            -----------------------------------------------------------'''
            #takes a rolling average of the measured speed and adds it to the run dataframe
            #roll_speed = run['speed'].rolling(5, 1, center = True).mean() 
            #run['roll_spd_avg'] = roll_speed
            '''-----------------------------------------------------------
            ---------------------Reilly Addition--------------------------
            -----------------------------------------------------------'''

            dfThresh = self.cropLower(run, 'force', fPercent)
            if len(dfThresh) > 0: # must not be empty
                trim = [ #gets first and last occurance of force value within fPercent of max force value
                    dfThresh.iloc[0]['time'],
                    dfThresh.iloc[0]['force'],
                    dfThresh.iloc[-1]['time'],  
                    dfThresh.iloc[-1]['force']  
                ]

                self.avgTruncList.append(trim)

                #if self.totalDistance(dfThresh) / self.totalDistance(self.df) < 0.4:
                #    continue #doesn't include data if less than 40% total stroke length

                #trim the data to be averaged
                #This part trims the dataframe to the timestamps extracted above
                dfTrunc = run[run['time'] >= dfThresh.iloc[0]['time']]
                dfTrunc = dfTrunc[dfTrunc['time'] <= dfThresh.iloc[-1]['time']] 

                #actual data averaging here:
                dist = dfTrunc['position'].iloc[0] - dfTrunc['position'].iloc[dfTrunc.shape[0] - 1] #distance traveled under load
                time = dfTrunc['time'].iloc[dfTrunc.shape[0] - 1] - dfTrunc['time'].iloc[0] # time under load
                if time == 0:
                    avg = [
                        dfTrunc['force'].mean(), #avg force under load
                        dfTrunc['current'].mean(), # avg current under load
                        0, #avg speed under load
                        time, # time under load
                        dist #distance under load
                    ]
                else:
                    avg = [
                        dfTrunc['force'].mean(), #avg force under load
                        dfTrunc['current'].mean(), # avg current under load
                        dist/time, #avg speed under load
                        time, # time under load
                        dist #distance under load
                    ]
                avgList.append(avg) #add avgs for run to list
        #return a new dataframe of the avgs of each run
        return pd.DataFrame(avgList, columns= ['Avg Load (N)', 'Avg Current (mA)', 'Avg Speed (mm/s)', 'Time (s)', 'Distance (mm)'])

    #drops rows where a column value is not within a percentage of the maximum column value
    #this algorithm needs to be more robust. maybe use some form of rolling avg max, because max value can be noisy at low loads
    def cropLower(self, df_in, col, percent):
        '''--------------------------------------------------------------
        -------------------Start Reilly Changes--------------------------
        -----------------------------------------------------------------'''
        AVERAGES = 5    #originally 4

        #indices = df_in[df_in[col] > df_in['target load']].index.tolist()  #for debugging only
        #targetf = df_in['target load'][0]                                  #for debugging only
        #New Reilly Method, better data range clipped to speed max(target speed)
        #Speed 1mm/s-<40 , speed doesn't decreease through magramp() but force does. Do check to cutoff data range at maglim position?
        if 'target speed' in df_in:
            speed = df_in['position'].diff() / -df_in['time'].diff()    #Calculates speed
            if df_in['stall'].iloc[-1] == 1:
                dfCrop = df_in.where(speed >= speed.max()*.5).dropna()  #Set dfCrop to a range of 50% of the max recorded speed
                if dfCrop.len() == 0:
                    dfCrop = df_in  #This may need adjusting later if we care about stall averages
                return dfCrop
            if df_in['target speed'][0] >= 40:          
                #dfCrop = df_in.where(speed.between(df_in['target speed'] - 4, df_in['target speed'] + 4)).dropna()
                dfCrop = df_in.where(speed.between(speed.max()-4, speed.max()+4)).dropna()      #Set dfCrop to a range between +/-4mm/s of the max calculated speed
                return dfCrop
            #elif df_in['target speed'][0] >= 5:

        #Old Reilly Method
        #if less than 5 values are greater than target force
        #TO DO: add check for if these values are consecutive and at the beginning of stroke and remove only early consecutive values causing spike
        if (df_in[col] > df_in['target load'][0]).sum() < 5:
            df_in_mask = df_in.where(df_in[col] <= df_in['target load'])
            dfAve = df_in_mask[col].rolling(AVERAGES, 1, center = True).mean()
            dfCrop = df_in[dfAve > dfAve.max() * (1 - percent)]
            #If dfCrop size is less 4 and run stalled, actuator stalled at the beginning of the stroke, return dfCrop unmodified
            if dfCrop.shape[0] < 4 and 'stall' in df_in and df_in['stall'].iloc[-1] == 1:
                return dfCrop
            #Excludes measured force values above target force in dfCrop if dfAve > dfAve.max() * (1 - percent) if 1st dfCrop value is greater
            #TO DO: add check for if these values are consecutive and at the beginning and remove only early consecutive values causing spike
            if dfCrop['force'].iloc[0] > df_in['target load'][0]: 
                #gets list of indices where drCrop['force'] is greater than target force   
                indices = dfCrop[dfCrop[col] > dfCrop['target load']].index.tolist()  
                #removes indices of dfCrop where measured force is greater than target force 
                dfCrop = dfCrop.drop(indices)
            #Special range for speeds < 30
            if 'target speed' in df_in and df_in['target speed'][0] < 30:
                #gets last 2 indexes in dfCrop to clip dfCrop to desired end range for force averaging
                return dfCrop.iloc[:-2, :]
            return dfCrop    
        
        #if 5 or more values are above target force   
        # #TO DO: add check for if these values are consecutive and at the beginning of stroke and remove early consecutive values causing spike 
        dfAve = df_in[col].rolling(AVERAGES, 1, center = True).mean()
        dfCrop = df_in[dfAve > dfAve.max() * (1 - percent)]
        if 'stall' in df_in and df_in['stall'].iloc[-1] == 1:
            return dfCrop
        if dfCrop.shape[0] > 7 and 'target speed' in df_in and df_in['target speed'][0] < 30: #maybe add a condition for less than speed 30 as well to prevent over clipping
                #gets last 3 indexes in dfCrop to clip dfCrop to desired end range for force averaging
                return dfCrop.iloc[3:-3, :]
        
        if 'stall' in df_in and df_in['stall'].iloc[-1] == 0 and dfCrop['time'].iloc[-1] < df_in['time'].iloc[-1]*0.75:
            return dfCrop.iloc[:-1, :]
       
        return dfCrop

        '''---------------------------------------------------------------
            ---------------------End Reilly Changes---------------------------
            ------------------------------------------------------------------'''
        
        '''#original code with original window and percent, this may need tweaking for range detection
        dfAve = df_in[col].rolling(4, 1).mean()  
        dfCrop = df_in[dfAve > dfAve.max() * (1 - 0.1)]

        return dfCrop '''

    def totalDistance(self, df_in): #returns an the delta between start and end position of dataframe
        return df_in['position'].max() - df_in['position'].min()


#inherited class of csvProcessor, specific to ramp test
class rampProcessor(csvProcessor):

    specPath = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', 'Test Results', 'ForceCheck.xlsx'))

    def __init__(self, fname, complete_dict):
        super(rampProcessor, self).__init__(fname, complete_dict)
        self.model = complete_dict['actuator']['model'][0:-2]
        self.dfSpecs = self.getSpecs(self.model)
        self.checkUp = self.dataCompare()

    #looks up the model of actuator and returns upper and lower threshold data
    def getSpecs(self, model):
        try:
            dfSpecs = pd.read_excel(self.specPath)
            dfSpecs = dfSpecs[dfSpecs['Actuator Model'].str.contains(model)]
            self.minforce = dfSpecs['Min Force'].iloc[0]
            m_speed = dfSpecs['m (speed)'].iloc[0]
            c_speed = dfSpecs['c (speed)'].iloc[0]
            m_current = dfSpecs['m (current)'].iloc[0]
            c_current = dfSpecs['c (current)'].iloc[0]
            speed_tol = dfSpecs['Speed Tolerance (mm/s)'].iloc[0]
            cur_tol = dfSpecs['Current Tolerance (mA)'].iloc[0]
        
        except IndexError:
            print('IndexError while reading ForceCheck.xlsx - The model under test may not be listed there')

        dataTrunc = self.dfAvg[self.dfAvg['Avg Load (N)'] <= self.minforce] #data above max rated force is excluded from checks

        rangeList = []
        for row in dataTrunc.index: #should consider switching to using the "apply()" Pandas method rather than iterating
            speed = dataTrunc['Avg Load (N)'].iloc[row] * m_speed + c_speed
            speedH = speed + speed_tol
            speedL = speed - speed_tol
            current = dataTrunc['Avg Load (N)'].iloc[row] * m_current + c_current
            currentH = current + cur_tol
            currentL = current - cur_tol
            rangeList.append([speed, speedH, speedL, current, currentH, currentL])
        #expected speed and current consumption data is generated
        return pd.DataFrame(rangeList, columns= ['speed', 'speed + tol', 'speed - tol', 'current', 'current + tol', 'current - tol'])

    #compares avg run data to a set of predetermined standards
    def dataCompare(self):
        #Minimum force check
        if self.dfAvg['Avg Load (N)'].max() < self.minforce:
            forcecheck = 'FAIL'
        else:
            forcecheck = 'PASS'

        #current check
        df = pd.concat([self.dfAvg, self.dfSpecs], axis=1, join='inner')
        if((df['Avg Current (mA)']>df['current - tol']).all()
            and (df['Avg Current (mA)']<df['current + tol']).all()):
            currentcheck = 'PASS'
        else:
            currentcheck = 'FAIL'

        #speed check
        if((df['Avg Load (N)'] > df['speed - tol']).all()
            and (df['Avg Load (N)'] < df['speed + tol']).all()):
            speedcheck = 'PASS'
        else:
            speedcheck = 'FAIL'

        #Max force
        maxforce = self.dfAvg['Avg Load (N)'].max()
        #current @max force
        maxfcurrent = self.dfAvg[self.dfAvg['Avg Load (N)'] == maxforce]['Avg Current (mA)'].iat[0]
        #speed @max force
        maxfspeed = self.dfAvg[self.dfAvg['Avg Load (N)'] == maxforce]['Avg Speed (mm/s)'].iat[0]
        #no load speed approximation (uses data <= min force spec because highre forces are less linear)
        x = self.dfAvg[self.dfAvg['Avg Load (N)'] <= self.minforce]['Avg Load (N)']
        y = self.dfAvg[self.dfAvg['Avg Load (N)'] <= self.minforce]['Avg Speed (mm/s)']
        if x.empty or y.empty:
            print('Error: Cannot perform no-load approximation - all measured force values are above "Min Force" threshold in ForceCheck.xlsx')
            nlspeed = 0
        else:
            nlspeed = np.polyfit(x, y, 1)[1]

        #returns list of pass/fail and specs, should maybe consider returning a dictionairy instead to make it very clear what each item is
        return [forcecheck, currentcheck, speedcheck, maxforce, maxfcurrent, maxfspeed, nlspeed]


#inherited class of csvProcessor, specific to the calibration check procedure.
class calProcessor(csvProcessor):

    CURRENT_CHECK = [200, 200, 100] #current measured in mA at 12V,12V,6V (60ohm resistor)
    FORCE_CHECK = [9.81 * 0.5, 20 * 4.448, 65.25 * 4.448] #500g,20.00lb,65.25lb (in Newtons)

    CURRENT_ACCURACY = 0.02 #must be within 2% accuracy
    FORCE_ACCURACY = 0.02

    def __init__(self, fname):
        super(calProcessor,self).__init__(fname)
        self.dfCal = self.calCheck(self.dfAvg) #create a dataframe of calibration data from run averaged data
        self.curResult = self.accCheck(self.dfCal, 'Current %Err.', self.CURRENT_ACCURACY) #check the current readings
        self.forceResult = self.accCheck(self.dfCal,'Force %Err.', self.FORCE_ACCURACY) #check the loadcell readings

    #generates a dataframe of measured and expected values for comparison
    def calCheck(self, df_in):
        df_in = df_in.drop(['Avg Speed (mm/s)', 'Time (s)', 'Distance (mm)'], axis=1) #remove un-needed columns
        df_in['Expected (mA)'] = self.CURRENT_CHECK #use class attributes defined above
        df_in['Current %Err.'] = (df_in['Avg Current (mA)'] - df_in['Expected (mA)']) / df_in['Expected (mA)'] #calculate the error in the current measured
        df_in['Expected (N)'] = self.FORCE_CHECK #use class attributes defined above
        df_in['Force %Err.'] = (df_in['Avg Load (N)'] - df_in['Expected (N)']) / df_in['Expected (N)'] #calculate the error in the force measured
        df_in = df_in.reindex(columns=['Avg Load (N)', 'Expected (N)', 'Force %Err.', 'Avg Current (mA)', 'Expected (mA)', 'Current %Err.']) #rearrange column order
        return df_in

    #checks the accuracy for a spefified accuracy column
    def accCheck(self,df_in,col,acc):
        if((df_in[col] < acc).all() and (df_in[col] > -acc).all()):
            return 'PASS'
        else:
            return 'FAIL'


#still a work in progress
#aims to merge the max force test and ramp test results so that the ramp data can be extended further
class maxfProcessor(rampProcessor):

    def __init__(self, maxfCSV, rampCSV, model):
        super(maxfProcessor, self).__init__(rampCSV, model)
        self.maxfData = csvProcessor(maxfCSV)
        self.df = self.mergeDF(self.df, self.maxfData.df)
        self.dfRuns = self.splitRuns(self.df)
        self.dfAvg = self.runAvg(0.16)                      #Reilly change: originally 0.1
        self.dfSpecs = self.getSpecs(model)
        self.checkUp = self.dataCompare()

    def mergeDF(self, df, dfMaxf):
        dfMaxf['run'] += df['run'].max()
        return pd.concat([df, dfMaxf], ignore_index=True)

# currently can't inherit from base class, methods don't work on this dataframe
class stepperSpeedProcessor(csvProcessor):
    def __init__(self, fpath, complete_dict):
        self.df = csvProcessor(fpath, complete_dict).df
        self.df['speed'] = -self.df.groupby('run')['position'].diff() / self.df.groupby('run')['time'].diff()  # add speed column from dPosition / dTime (negative because actuator is retracing)
        self.dfRuns = self.splitRuns(self.df)
        self.dfAvg = self.runAverage('force', 0.16)         #Reilly change: originally 0.1

    def runAverage(self, col_name, fPercent): #takes a truncated average of a test run dataframe. Data starts at first instance of force within 100-fPercent of maximum force in the run
        avgList = []
        self.avgTruncList = []
        for run in self.dfRuns:
            #dfThresh = self.remove_outlier(run,'force',0,fPercent)
            dfThresh = self.cropLower(run, col_name, fPercent)
            if len(dfThresh) > 0: # must not be empty
                trim = [ #gets first and last occurance of force value within fPercent of max force value
                    dfThresh['time'].iloc[0],
                    dfThresh[col_name].iloc[0],
                    dfThresh['time'].iloc[-1],      
                    dfThresh[col_name].iloc[-1]     
                ]

                self.avgTruncList.append(trim)

                #if self.totalDistance(dfThresh)/self.totalDistance(self.df) < 0.4:
                #    continue #doesn't include data if less than 40% total stroke length

                #trim the data to be averaged
                #This part trims the dataframe to the timestamps extracted above
                dfTrunc = run[run['time'] >= dfThresh['time'].iat[0]]
                dfTrunc = dfTrunc[dfTrunc['time'] <= dfThresh['time'].iat[-1]]  

                
                
                #actual data averaging here:
                time = dfTrunc['time'].iat[dfTrunc.shape[0]-1]-dfTrunc['time'].iat[0] # time under load
                avg = [
                    #dfTrunc['run'].mean(),     # TODO: run is getting added to spreadsheet elsewhere - in Excel code?
                    time,
                    2 ** dfTrunc['step mode'].mean(),    # mean may not be necessary since these are supposed to be constants
                    dfTrunc['target speed'].mean(),                         # ^^
                    dfTrunc['speed'].mean(),                      # avg speed under load
                    dfTrunc['target load'].mean(),                         # ^^
                    dfTrunc['force'].mean(),    # avg force under load                 
                    dfTrunc['current'].mean(),  # avg current under load
                    1 if run['stall'].mean() > 0 else 0    # if there was a stall, the mean will be non-zero.  Round up to 1 for clarity's sake
                ]
                avgList.append(avg) #add avgs for run to list

        #return a new dataframe of the avgs of each run
        return pd.DataFrame(avgList, columns=['Time (s)', 'Step Mode (1/x)', 'Target Speed (mm/s)', 'Avg Speed (mm/s)', 
                                                'Target Load (N)', 'Avg Load (N)', 'Avg Current (mA)', 'Stall'])

# test code
if __name__ == '__main__':
    from excelWriter import *
    #doc = excelDoc('C:\\Users\\elec\Desktop\\test.xlsx')
    #x = stepperSpeedProcessor('C:\\Users\\elec\Desktop\\stepper_speed_data.csv')
    #stepperSpeedSheet(doc, x.df, x.dfAvg, '')
    #[stepperRunSheet(doc, x.dfRuns[n], x.avgTruncList[n]) for n in range(x.dfAvg.shape[0])]
    #doc.saveDoc()
    #print(x.dfRuns)
    #print(x.avgTruncList)
    complete_dict = {
        "actuator": {
            "model": "P16-50-256-12-S",
            "motor": "BRUSHED",
            "stroke": 100,
            "gear": 256,
            "voltage": 12,
            "type": "S"
        },
        "rig": {
            "rewind": 3200,
            "decog": 50,
            "maglim": 950000,
            "extlim": 1030000,
            "retlim": 1410000,
            "reel": "A",
            "hangwt": "0"
        },
        "test": {
            "duty": "100",
            "speed": 100,
            "extend": 100,
            "retract": 100,
            "format": "RAMP",
            "startload": "10",
            "endload": "150",
            "runs": "6",
            "findmax": "0",
            "warmup": "0",
            "stallcrnt": "0",
            "noload": "0"
        },
        "brake":
        {
            "A": [0, 0.006341],
            "B": [0, 0.000014],
            "C": [0, 0.000394],
            "D": [0, 0.130812],
            "F": [0, 0.141711],
            "T": [0, -17.3344960],
            "max": [0, 1000],
            "min": [0, 3.3]
        },
        
        "loadcell": 
        {
            "A_high": -9.264e-23,
            "B_high": 2.76e-16,
            "C_high": -2.9036e-10,
            "D_high": 6.1904e-4,
            "E_high": -1.1419e2,
            "threshold": 241688,
            "A_low": -1.61985e-17,
            "B_low": 1.46199e-11,
            "C_low": -4.94439e-6,
            "D_low": 7.43164e-1,
            "E_low": -4.19038e4,
            "max_load": 200
        },

        "posSensor":
        {
            "scale": 36500
        }
    }

    data = rampProcessor('C:\\Users\\elec\\repos\\PerformanceTestRig_App\\Python\\ramp_data.csv', complete_dict)
    Doc = excelDoc('output.xlsx')
    ramp = rampSheet(Doc, data.dfAvg, data.dfSpecs, data.checkUp, '')
    
    #Generate individual run data in report if requested
    for i, run in enumerate(data.dfRuns):
        runSheet(Doc, run, data.avgTruncList[i],i+1)
    Doc.saveDoc()