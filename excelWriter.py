import pandas as pd

# 1. Convert your CSV into an Excel file
#excel_file = 'results.xlsx'
#df = pd.read_csv('data.csv')
#df['Time'] = (df['Time']-df['Time'].iloc[0])*0.001

def process_csv(df, excel_file):
    with pd.ExcelWriter(excel_file, engine='xlsxwriter') as writer:
        df.to_excel(writer, sheet_name='Results', index=False)
        workbook  = writer.book
        worksheet = writer.sheets['Results']



        chart = workbook.add_chart({'type': 'scatter','subtype': 'straight'})
        # Configure the series of the chart from the dataframe data.
        # Range format: [sheet_name, start_row, start_col, end_row, end_col]

        chart.add_series({
            'name': '740nm',
            'categories': ['Results', 2, 0, len(df)-1, 0],
            'values': ['Results', 2, 1, len(df)-1, 1],
            'line': {'color': 'green'},
        })

        chart.add_series({
            'name': '850nm',
            'categories': ['Results', 2, 0, len(df)-1, 0],
            'values': ['Results', 2, 2, len(df)-1, 2],
            'line': {'color': 'red'},
            'y2_axis': 1,
        })
        
        chart.set_title({'name': 'TIA ADC Readings'})
        chart.set_x_axis({'name': 'Time (s)'})
        chart.set_y_axis({'name': 'Active DC ADC'})
        chart.set_y2_axis({'name': 'Pulse AC ADC'})
        chart.set_size({"width": 1000, "height": 400})
        worksheet.insert_chart('M2', chart)


        chart = workbook.add_chart({'type': 'scatter','subtype': 'straight'})
        chart.add_series({
            'name': '740nm',
            'categories': ['Results', 2, 0, len(df)-1, 0],
            'values': ['Results', 2, 3, len(df)-1, 3],
            'line': {'color': 'green'},
        })

        chart.add_series({
            'name': '850nm',
            'categories': ['Results', 2, 0, len(df)-1, 0],
            'values': ['Results', 2, 4, len(df)-1, 4],
            'line': {'color': 'red'},

        })
        chart.set_title({'name': 'Voltage Follower ADC Readings'})
        chart.set_x_axis({'name': 'Time (s)'})
        chart.set_y_axis({'name': 'ADC'})
        chart.set_size({"width": 1000, "height": 400})
        worksheet.insert_chart('M23', chart)


        chart = workbook.add_chart({'type': 'scatter','subtype': 'straight'})
        chart.add_series({
            'name': 'HbR',
            'categories': ['Results', 1, 0, len(df)-1, 0],
            'values': ['Results', 1, 5, len(df)-1, 5],
            'line': {'color': 'green'},
        })

        chart.add_series({
            'name': 'HbO2',
            'categories': ['Results', 2, 0, len(df)-1, 0],
            'values': ['Results', 2, 6, len(df)-1, 6],
            'line': {'color': 'red'},

        })
        chart.set_title({'name': 'Oygenation Readings'})
        chart.set_x_axis({'name': 'Time (s)'})
        chart.set_y_axis({'name': 'ADC'})
        chart.set_size({"width": 1000, "height": 400})
        worksheet.insert_chart('M44', chart)
        