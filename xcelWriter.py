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
            'name': 'AmbientDC',
            'categories': ['Results', 2, 0, len(df)-1, 0],
            'values': ['Results', 2, 1, len(df)-1, 1],
            'line': {'color': 'blue'},
        })

        chart.add_series({
            'name': 'AmbientAC',
            'categories': ['Results', 2, 0, len(df)-1, 0],
            'values': ['Results', 2, 2, len(df)-1, 2],
            'line': {'color': 'red','dash_type': 'dash','width':2},
        })
        
        chart.set_title({'name': 'Ambient Dark Current'})
        chart.set_x_axis({'name': 'Time (s)'})
        chart.set_y_axis({'name': 'Ambient ADC'})
        worksheet.insert_chart('M2', chart)

