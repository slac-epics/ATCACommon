from pydm import Display
import json
import logging
import sys
import pyqtgraph as pg

logger = logging.getLogger(__name__)


class Scope(Display):
    def __init__(self, parent=None, args=None, macros=None, ui_filename=None):
        super(Scope, self).__init__(parent=parent, args=args, macros=macros)
        self._macros = macros
        self._curves = None
        self.define_curves()
        self.setup_ui()
        self.setup_curve_selection_mode()

        try:
            self._macros["DEVICE"]
        except:
            logger.error(
                "Please provide a valid macro "
                "for the IOC, ex: 'DEVICE=MY_IOC'")
            sys.exit(1)

    def define_curves(self):
        try:
            device = self._macros["DEVICE"]
            if device:
                ioc = "ca://{}:".format(device)

                bays = [0, 1]
                channels = [0, 1, 2, 3]
                colors = ["#55ffff", "#55ff7f", "#ffff7f", "#ffaa7f"]

                self._curves = {}
                for b_i, bay in enumerate(bays):
                    bay_data = {}
                    for ch in channels:
                        y_channel = "{}STR{}:STREAM_SLOWDOUBLE{}".format(
                            ioc, b_i, ch)
                        name = "CH {}".format(ch)

                        bay_data[ch] = {
                            "y_channel": y_channel,
                            "x_channel": None,
                            "name": name,
                            "color": colors[ch]
                        }

                    self._curves[b_i] = bay_data
        except:
            logger.error(
                "You need to define a DEVICE macro, ex: -m 'DEVICE=MY_IOC' ")
            sys.exit(1)

    def handle_show_curve_bay0(self):
        # bay 0
        b0_curves = []
        if self._curves:
            bay0_curves = self._curves[0]

            style = {
                "lineStyle": 1, "lineWidth": 1, "symbol": 0,
                "symbolSize": 4, "redraw_mode": 2
            }

            if self.ui.bay0Mode0_rb.isChecked():
                style = {
                    "lineStyle": 0, "lineWidth": 1, "symbol": 0,
                    "symbolSize": 4, "redraw_mode": 2
                }

            b0_pbs = [self.ui.b0_channel0_pb, self.ui.b0_channel1_pb,
                      self.ui.b0_channel2_pb, self.ui.b0_channel3_pb]

            for idx, button in enumerate(b0_pbs):
                if button.isChecked():
                    curve = bay0_curves.get(idx)
                    curve.update(style)
                    ch = json.dumps(curve)
                    b0_curves.append(ch)
            self.waveformPlotBay0.setCurves(b0_curves)

    def handle_show_curve_bay1(self):
        # bay 1
        b1_curves = []
        if self._curves:
            bay1_curves = self._curves[1]

            style = {
                "lineStyle": 1, "lineWidth": 1, "symbol": 0,
                "symbolSize": 4, "redraw_mode": 2
            }

            if self.ui.bay1Mode0_rb.isChecked():
                style = {
                    "lineStyle": 0, "lineWidth": 1, "symbol": 0,
                    "symbolSize": 4, "redraw_mode": 2
                }

            b1_pbs = [self.ui.b1_channel0_pb, self.ui.b1_channel1_pb,
                      self.ui.b1_channel2_pb, self.ui.b1_channel3_pb]

            for idx, button in enumerate(b1_pbs):
                if button.isChecked():
                    curve = bay1_curves.get(idx)
                    curve.update(style)
                    ch = json.dumps(curve)
                    b1_curves.append(ch)
            self.waveformPlotBay1.setCurves(b1_curves)

    def setup_curve_selection_mode(self):
        self.waveform0Ch0.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveform0Ch1.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveform0Ch2.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveform0Ch3.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveform1Ch0.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveform1Ch1.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveform1Ch2.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveform1Ch3.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveformPlotBay0.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)
        self.waveformPlotBay1.plotItem.getViewBox().setMouseMode(
            pg.ViewBox.RectMode)

    def setup_ui(self):
        # bay 0
        self.ui.b0_channel0_pb.clicked.connect(self.handle_show_curve_bay0)
        self.ui.b0_channel1_pb.clicked.connect(self.handle_show_curve_bay0)
        self.ui.b0_channel2_pb.clicked.connect(self.handle_show_curve_bay0)
        self.ui.b0_channel3_pb.clicked.connect(self.handle_show_curve_bay0)
        # bay 1
        self.ui.b1_channel0_pb.clicked.connect(self.handle_show_curve_bay1)
        self.ui.b1_channel1_pb.clicked.connect(self.handle_show_curve_bay1)
        self.ui.b1_channel2_pb.clicked.connect(self.handle_show_curve_bay1)
        self.ui.b1_channel3_pb.clicked.connect(self.handle_show_curve_bay1)
        # modes
        self.ui.bay0Mode0_rb.clicked.connect(self.handle_show_curve_bay0)
        self.ui.bay0Mode1_rb.clicked.connect(self.handle_show_curve_bay0)
        self.ui.bay1Mode0_rb.clicked.connect(self.handle_show_curve_bay1)
        self.ui.bay1Mode1_rb.clicked.connect(self.handle_show_curve_bay1)

    def ui_filename(self):
        return 'dualScope.ui'
