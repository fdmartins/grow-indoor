# -*- coding: utf-8 -*-

# Run this app with `python app.py` and
# visit http://127.0.0.1:8050/ in your web browser.

import random
import time
import dash
import dash_core_components as dcc
import dash_html_components as html
from dash.dependencies import Input, Output, State
from dash.exceptions import PreventUpdate
from threading import Thread
import serial
import datetime

# iniciamos thread que captura dados do arduino.
class GROW_INDOOR:
    def __init__(self):
        print("INICIADO GROW_INDOOR")
        self.running = True

        self.ser = serial.Serial()
        self.ser.port = '/dev/ttyUSB0'
        self.ser.baudrate = 9600
        self.ser.bytesize = serial.EIGHTBITS #number of bits per bytes
        self.ser.parity = serial.PARITY_NONE #set parity check: no parity
        self.ser.stopbits = serial.STOPBITS_ONE #number of stop bits
        self.ser.timeout = None          #block read
        #self.ser.timeout = 0            # non blocking read
        self.ser.xonxoff = False     #disable software flow control
        self.ser.rtscts = False     #disable hardware (RTS/CTS) flow control
        self.ser.dsrdtr = False       #disable hardware (DSR/DTR) flow control
        self.ser.writeTimeout = 2     #timeout for write

        self.ser.open()
        self.ser.flushInput()

        self.data = {}

        self.arduinoMonitor = Thread(target=self.arduinoProcess,args=[])
        self.arduinoMonitor.daemon=True

        self.arduinoMonitor.start()

    def getData(self):
        return self.data

    def stop(self):
        self.running = False

    def arduinoProcess(self):
        lasthourupdate = 0

        while self.running:
            
            try:
                lineArduino = self.ser.readline().decode('utf-8').strip()

                if lineArduino[0]!="$":
                    print(lineArduino)
                    continue
                    
                print(lineArduino)

                # respondemos com o horario atual a cada hora, para sincronizar.
                now = datetime.datetime.now()
            
                hour = now.hour
                minute = now.minute
                if lasthourupdate!=minute:
                    print("atualizando horario:", hour, minute)
                    self.ser.write([hour,minute])
                    lasthourupdate = minute
                

                try:
                    data = [v for v in lineArduino.split(', ')]
                    labels = [d.split(':')[0] for d in data[1:]]
                    time = now #data[0].split(':')[1]
                    values = [float(d.replace(',','').split(':')[1]) for d in data[1:]]
                    #print(labels)
                    #print(values)

                    for i in range(len(labels)):
                        label = labels[i]
                        value = values[i]

                        if label not in self.data:
                            self.data.setdefault(label, [])

                        
                        while len(self.data[label])>1500:
                            self.data[label].pop(0)
                            
                        self.data[label].append((time, value))
                                            

                except Exception as e1:
                    print(e1)

            except Exception as er:
                print(er)

        print("== FINALIZADO ==")
    





# iniciamos servidor dashboard de visualizacao.

external_stylesheets = ['https://codepen.io/chriddyp/pen/bWLwgP.css']

app = dash.Dash(__name__, external_stylesheets=external_stylesheets)


app.layout = html.Div(children=[
    html.H1(children='Indoor Grow', id='first'),

    dcc.Interval(id='timer', interval=1000),


    html.Div(id='dummy'),

    dcc.Graph(
            id='sensors-graph',
           
        )
])


@app.callback(output=Output('sensors-graph', 'figure'),
              inputs=[Input('timer', 'n_intervals')])
def update_graph(n_clicks):

    dataDict = grow.getData()

    data = []

    for label, values in dataDict.items():
        #print(label, values)
        data.append( {'x': [values[i][0] for i in range(len(values))] , 'y': [values[i][1] for i in range(len(values))], 'type': 'line', 'name': label } )

    #print(data)

    return {
                'data': data,
                'layout': {
                    'title': 'Sensores'
                }
            }

if __name__ == '__main__':
    grow = GROW_INDOOR()
    app.run_server(host="0.0.0.0", debug=False, threaded=True) # Rodar em Debug causa problema de abrir multiplas instancia da serial!

grow.stop()