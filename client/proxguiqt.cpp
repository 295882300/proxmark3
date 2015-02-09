//-----------------------------------------------------------------------------
// Copyright (C) 2009 Michael Gernoth <michael at gernoth.net>
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// GUI (QT)
//-----------------------------------------------------------------------------

#include <iostream>
#include <QPainterPath>
#include <QBrush>
#include <QPen>
#include <QTimer>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <QSlider>
#include <QHBoxLayout>
#include <string.h>
#include "proxguiqt.h"
#include "proxgui.h"
#include <QtGui>
//#include "data_operations.h"
#include <ctime>

int GridOffset= 0;
bool GridLocked= 0;
int startMax;
int PageWidth;

void ProxGuiQT::ShowGraphWindow(void)
{
	emit ShowGraphWindowSignal();
}

void ProxGuiQT::RepaintGraphWindow(void)
{
	emit RepaintGraphWindowSignal();
}

void ProxGuiQT::HideGraphWindow(void)
{
	emit HideGraphWindowSignal();
}

void ProxGuiQT::_ShowGraphWindow(void)
{
	if(!plotapp)
		return;

	if (!plotwidget)
		plotwidget = new ProxWidget();

	plotwidget->show();
}

void ProxGuiQT::_RepaintGraphWindow(void)
{
	if (!plotapp || !plotwidget)
		return;

	plotwidget->update();
}

void ProxGuiQT::_HideGraphWindow(void)
{
	if (!plotapp || !plotwidget)
		return;

	plotwidget->hide();
}

void ProxGuiQT::MainLoop()
{
	plotapp = new QApplication(argc, argv);

	connect(this, SIGNAL(ShowGraphWindowSignal()), this, SLOT(_ShowGraphWindow()));
	connect(this, SIGNAL(RepaintGraphWindowSignal()), this, SLOT(_RepaintGraphWindow()));
	connect(this, SIGNAL(HideGraphWindowSignal()), this, SLOT(_HideGraphWindow()));

	plotapp->exec();
}

ProxGuiQT::ProxGuiQT(int argc, char **argv) : plotapp(NULL), plotwidget(NULL),
	argc(argc), argv(argv)
{

}

ProxGuiQT::~ProxGuiQT(void)
{
	if (plotwidget) {
		delete plotwidget;
		plotwidget = NULL;
	}

	if (plotapp) {
		plotapp->quit();
		delete plotapp;
		plotapp = NULL;
	}
}

//--------------------

void ProxWidget::applyOperation()
{
	printf("ApplyOperation()");
	memcpy(GraphBuffer,s_Buff, sizeof(int) * GraphTraceLen);
	RepaintGraphWindow();

}
void ProxWidget::stickOperation()
{
	printf("stickOperation()");
}
void ProxWidget::vchange_autocorr(int v)
{
	autoCorr(GraphBuffer,s_Buff, GraphTraceLen, v);
	printf("vchange_autocorr(%d)\n", v);
	RepaintGraphWindow();
}
void ProxWidget::vchange_dthr_up(int v)
{
	int down = opsController->horizontalSlider_dirthr_down->value();
	directionalThreshold(GraphBuffer,s_Buff, GraphTraceLen, v, down);
	printf("vchange_dthr_up(%d)", v);
	RepaintGraphWindow();

}
void ProxWidget::vchange_dthr_down(int v)
{
	printf("vchange_dthr_down(%d)", v);
	int up = opsController->horizontalSlider_dirthr_up->value();
	directionalThreshold(GraphBuffer,s_Buff, GraphTraceLen, v, up);
	RepaintGraphWindow();

}
ProxWidget::ProxWidget(QWidget *parent, ProxGuiQT *master) : QWidget(parent)
{
	this->master = master;
	resize(800,500);

	/** Setup the controller widget **/

	QWidget* controlWidget = new QWidget();
	opsController = new Ui::Form();
	opsController->setupUi(controlWidget);
	//Due to quirks in QT Designer, we need to fiddle a bit
	opsController->horizontalSlider_dirthr_down->setMinimum(-128);
	opsController->horizontalSlider_dirthr_down->setMaximum(0);
	opsController->horizontalSlider_dirthr_down->setValue(-20);


	QObject::connect(opsController->pushButton_apply, SIGNAL(clicked()), this, SLOT(applyOperation()));
	QObject::connect(opsController->pushButton_sticky, SIGNAL(clicked()), this, SLOT(stickOperation()));
	QObject::connect(opsController->horizontalSlider_window, SIGNAL(valueChanged(int)), this, SLOT(vchange_autocorr(int)));
	QObject::connect(opsController->horizontalSlider_dirthr_up, SIGNAL(valueChanged(int)), this, SLOT(vchange_dthr_up(int)));
	QObject::connect(opsController->horizontalSlider_dirthr_down, SIGNAL(valueChanged(int)), this, SLOT(vchange_dthr_down(int)));

	controlWidget->show();

	// Set up the plot widget, which does the actual plotting

	plot = new Plot(this);
	/*
	QSlider* slider = new QSlider(Qt::Horizontal);
	slider->setFocusPolicy(Qt::StrongFocus);
	slider->setTickPosition(QSlider::TicksBothSides);
	slider->setTickInterval(10);
	slider->setSingleStep(1);
	*/
	QVBoxLayout *layout = new QVBoxLayout;
	//layout->addWidget(slider);
	layout->addWidget(plot);
	setLayout(layout);
	printf("Proxwidget Constructor just set layout\r\n");
}

//----------- Plotting

int Plot::xCoordOf(int i, QRect r )
{
	return r.left() + (int)((i - GraphStart)*GraphPixelsPerPoint);
}

int Plot::yCoordOf(int v, QRect r, int maxVal)
{
	int z = (r.bottom() - r.top())/2;
	return -(z * v) / maxVal + z;
}

int Plot::valueOf_yCoord(int y, QRect r, int maxVal)
{
	int z = (r.bottom() - r.top())/2;
	return (y-z) * maxVal / z;
}
static const QColor GREEN  = QColor(100, 255, 100);
static const QColor RED = QColor(255, 100, 100);
static const QColor BLUE = QColor(100,100,255);
static const QColor GREY = QColor(240,240,240);

QColor Plot::getColor(int graphNum)
{
	switch (graphNum){
	case 0: return GREEN;//Green
	case 1: return RED;//Red
	case 2: return BLUE;//Blue
	default: return GREY;
	}
}

void Plot::PlotGraph(int *buffer, int len, QRect plotRect, QRect annotationRect,QPainter *painter, int graphNum)
{
	clock_t begin = clock();
	QPainterPath penPath;

	startMax = (len - (int)((plotRect.right() - plotRect.left() - 40) / GraphPixelsPerPoint));
	if(startMax < 0) {
		startMax = 0;
	}
	if(GraphStart > startMax) {
		GraphStart = startMax;
	}

	int vMin = INT_MAX, vMax = INT_MIN,vMean = 0, v = 0,absVMax = 0;
	int sample_index =GraphStart ;
	for( ; sample_index < len && xCoordOf(sample_index,plotRect) < plotRect.right() ; sample_index++) {

		v = buffer[sample_index];
		if(v < vMin) vMin = v;
		if(v > vMax) vMax = v;
		vMean += v;
	}

	vMean /= (sample_index - GraphStart);

	if(fabs( (double) vMin) > absVMax) absVMax = (int)fabs( (double) vMin);
	if(fabs( (double) vMax) > absVMax) absVMax = (int)fabs( (double) vMax);
	absVMax = (int)(absVMax*1.2 + 1);
	// number of points that will be plotted
	int span = (int)((plotRect.right() - plotRect.left()) / GraphPixelsPerPoint);
	// one label every 100 pixels, let us say
	int labels = (plotRect.right() - plotRect.left() - 40) / 100;
	if(labels <= 0) labels = 1;
	int pointsPerLabel = span / labels;
	if(pointsPerLabel <= 0) pointsPerLabel = 1;

	int x = xCoordOf(0, plotRect);
	int y = yCoordOf(buffer[0],plotRect,absVMax);
	penPath.moveTo(x, y);
	//int zeroY = yCoordOf( 0 , plotRect , absVMax);
	for(int i = GraphStart; i < len && xCoordOf(i, plotRect) < plotRect.right(); i++) {

		x = xCoordOf(i, plotRect);
		v = buffer[i];

		y = yCoordOf( v, plotRect, absVMax);//(y * (r.top() - r.bottom()) / (2*absYMax)) + zeroHeight;

		penPath.lineTo(x, y);

		if(GraphPixelsPerPoint > 10) {
			QRect f(QPoint(x - 3, y - 3),QPoint(x + 3, y + 3));
			painter->fillRect(f, QColor(100, 255, 100));
		}

/*		if(((i - GraphStart) % pointsPerLabel == 0) && i != GraphStart) {
			char str[100];
			sprintf(str, "+%d", (i - GraphStart));

			painter->setPen(QColor(255, 255, 255));
			QRect size;
			QFontMetrics metrics(painter->font());
			size = metrics.boundingRect(str);
			painter->drawText(x - (size.right() - size.left()), zeroY + 9, str);
		}
*/
	}

	painter->setPen(getColor(graphNum));

	//Draw y-axis
	int xo = 5+(graphNum*40);
	painter->drawLine(xo, plotRect.top(),xo, plotRect.bottom());

	int vMarkers = (absVMax - (absVMax % 10)) / 5;
	int minYDist = 40; //Minimum pixel-distance between markers

	char yLbl[20];

	int n = 0;
	int lasty0 = 65535;

	for(int v = vMarkers; yCoordOf(v,plotRect,absVMax) > plotRect.top() && n < 20; v+= vMarkers ,n++)
	{
		int y0 = yCoordOf(v,plotRect,absVMax);
		int y1 = yCoordOf(-v,plotRect,absVMax);

		if(lasty0 - y0 < minYDist) continue;

		painter->drawLine(xo-5,y0, xo+5, y0);

		sprintf(yLbl, "%d", v);
		painter->drawText(xo+5,y0-2,yLbl);

		painter->drawLine(xo-5, y1, xo+5, y1);
		sprintf(yLbl, "%d",-v);
		painter->drawText(xo+5, y1-2 , yLbl);
		lasty0 = y0;
	}


	//Graph annotations
	painter->drawPath(penPath);
	char str[200];
	sprintf(str, "max=%5d min=%5d mean=%5d n=%5d/%5d CursorA=[%5d] CursorB=[%5d]",
			vMax, vMin, vMean, sample_index, len, buffer[CursorAPos], buffer[CursorBPos]);
	painter->drawText(20, annotationRect.bottom() - 25 - 15 * graphNum, str);

	clock_t end = clock();
	double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
	printf("Plot time %f\n", elapsed_secs);
}
void Plot::plotGridLines(QPainter* painter,QRect r)
{
	int zeroHeight = r.top() + (r.bottom() - r.top()) / 2;

	int i;
	int grid_delta_x = (int) (PlotGridX * GraphPixelsPerPoint);
	int grid_delta_y = (int) (PlotGridY * GraphPixelsPerPoint);

	if (PlotGridX > 0 && grid_delta_x > 1) {
		for(i = 40 + (GridOffset * GraphPixelsPerPoint); i < r.right(); i += grid_delta_x)
			painter->drawLine(r.left()+i, r.top(), r.left()+i, r.bottom());
	}
	if (PlotGridY > 0 && grid_delta_y > 1){
		for(i = 0; i < ((r.top() + r.bottom())>>1); i += grid_delta_y) {
			painter->drawLine(r.left() + 40,zeroHeight + i,r.right(),zeroHeight + i);
			painter->drawLine(r.left() + 40,zeroHeight - i,r.right(),zeroHeight - i);

		}
	}

}
#define HEIGHT_INFO 60
#define WIDTH_AXES 80

void Plot::paintEvent(QPaintEvent *event)
{

	QPainter painter(this);

	QBrush brush(QColor(100, 255, 100));
	QPen pen(QColor(100, 255, 100));

	painter.setFont(QFont("Courier New", 10));

	if(GraphStart < 0) {
		GraphStart = 0;
	}

	if (CursorAPos > GraphTraceLen)
		CursorAPos= 0;
	if(CursorBPos > GraphTraceLen)
		CursorBPos= 0;


	QRect plotRect(WIDTH_AXES, 0, width()-WIDTH_AXES, height()-HEIGHT_INFO);
	QRect infoRect(0, height()-HEIGHT_INFO, width(), HEIGHT_INFO);

	//Grey background
	painter.fillRect(rect(), QColor(60, 60, 60));
	//Black foreground
	painter.fillRect(plotRect, QColor(0, 0, 0));

//	painter.fillRect(infoRect, QColor(60, 60, 60));

	int zeroHeight = plotRect.top() + (plotRect.bottom() - plotRect.top()) / 2;
	// Draw 0-line
	painter.setPen(QColor(100, 100, 100));
	painter.drawLine(plotRect.left(), zeroHeight, plotRect.right(), zeroHeight);
	// plot X and Y grid lines
	plotGridLines(&painter, plotRect);

	//Start painting graph
	PlotGraph(s_Buff, GraphTraceLen,plotRect,infoRect,&painter,1);
	PlotGraph(GraphBuffer, GraphTraceLen,plotRect,infoRect,&painter,0);
	// End graph drawing

	//Draw the two cursors
	if(CursorAPos > GraphStart && xCoordOf(CursorAPos, plotRect) < plotRect.right())
	{
		painter.setPen(QColor(255, 255, 0));
		painter.drawLine(xCoordOf(CursorAPos, plotRect),plotRect.top(),xCoordOf(CursorAPos, plotRect),plotRect.bottom());
	}
	if(CursorBPos > GraphStart && xCoordOf(CursorBPos, plotRect) < plotRect.right())
	{
		painter.setPen(QColor(255, 0, 255));
		painter.drawLine(xCoordOf(CursorBPos, plotRect),plotRect.top(),xCoordOf(CursorBPos, plotRect),plotRect.bottom());
	}

	//Draw annotations
	char str[200];
	sprintf(str, "@%5d  dt=%5d [%2.2f] zoom=%2.2f            CursorA= %5d  CursorB= %5d  GridX=%5d GridY=%5d (%s)",
			GraphStart,	CursorBPos - CursorAPos, (CursorBPos - CursorAPos)/CursorScaleFactor,
			GraphPixelsPerPoint,CursorAPos,CursorBPos,PlotGridXdefault,PlotGridYdefault,GridLocked?"Locked":"Unlocked");
	painter.setPen(QColor(255, 255, 255));
	painter.drawText(20, infoRect.bottom() - 10, str);

}


Plot::Plot(QWidget *parent) : QWidget(parent), GraphStart(0), GraphPixelsPerPoint(1)
{
	//Need to set this, otherwise we don't receive keypress events
	setFocusPolicy( Qt::StrongFocus);
	resize(600, 300);

	QPalette palette(QColor(0,0,0,0));
	palette.setColor(QPalette::WindowText, QColor(255,255,255));
	palette.setColor(QPalette::Text, QColor(255,255,255));
	palette.setColor(QPalette::Button, QColor(100, 100, 100));
	setPalette(palette);
	setAutoFillBackground(true);
	CursorAPos = 0;
	CursorBPos = 0;

	setWindowTitle(tr("Sliders"));


}

void Plot::closeEvent(QCloseEvent *event)
{
	event->ignore();
	this->hide();
}

void Plot::mouseMoveEvent(QMouseEvent *event)
{
	int x = event->x();
	x -= WIDTH_AXES;
	x = (int)(x / GraphPixelsPerPoint);
	x += GraphStart;
	if((event->buttons() & Qt::LeftButton)) {
		CursorAPos = x;
	} else if (event->buttons() & Qt::RightButton) {
		CursorBPos = x;
	}


	this->update();
}

void Plot::keyPressEvent(QKeyEvent *event)
{
	int	offset;
	int	gridchanged;

	gridchanged= 0;
	if(event->modifiers() & Qt::ShiftModifier) {
		if (PlotGridX)
			offset= PageWidth - (PageWidth % PlotGridX);
		else
			offset= PageWidth;
	} else 
		if(event->modifiers() & Qt::ControlModifier)
			offset= 1;
		else
			offset= (int)(20 / GraphPixelsPerPoint);

	switch(event->key()) {
		case Qt::Key_Down:
			if(GraphPixelsPerPoint <= 50) {
				GraphPixelsPerPoint *= 2;
			}
			break;

		case Qt::Key_Up:
			if(GraphPixelsPerPoint >= 0.02) {
				GraphPixelsPerPoint /= 2;
			}
			break;

		case Qt::Key_Right:
			if(GraphPixelsPerPoint < 20) {
				if (PlotGridX && GridLocked && GraphStart < startMax){
					GridOffset -= offset;
					GridOffset %= PlotGridX;
					gridchanged= 1;
				}
				GraphStart += offset;
			} else {
				if (PlotGridX && GridLocked && GraphStart < startMax){
					GridOffset--;
					GridOffset %= PlotGridX;
					gridchanged= 1;
				}
				GraphStart++;
			}
			if(GridOffset < 0) {
				GridOffset += PlotGridX;
			}
			if (gridchanged)
				if (GraphStart > startMax) {
					GridOffset += (GraphStart - startMax);
					GridOffset %= PlotGridX;
				}
			break;

		case Qt::Key_Left:
			if(GraphPixelsPerPoint < 20) {
				if (PlotGridX && GridLocked && GraphStart > 0){
					GridOffset += offset;
					GridOffset %= PlotGridX;
					gridchanged= 1;
				}
				GraphStart -= offset;
			} else {
				if (PlotGridX && GridLocked && GraphStart > 0){
					GridOffset++;
					GridOffset %= PlotGridX;
					gridchanged= 1;
				}
				GraphStart--;
			}
			if (gridchanged){
				if (GraphStart < 0)
					GridOffset += GraphStart;
				if(GridOffset < 0)
					GridOffset += PlotGridX;
			GridOffset %= PlotGridX;
			}
			break;

		case Qt::Key_G:
			if(PlotGridX || PlotGridY) {
				PlotGridX= 0;
				PlotGridY= 0;
			} else {
				PlotGridX= PlotGridXdefault;
				PlotGridY= PlotGridYdefault;
				}
			break;

		case Qt::Key_H:
			puts("Plot Window Keystrokes:\n");
			puts(" Key                      Action\n");
			puts(" DOWN                     Zoom in");
			puts(" G                        Toggle grid display");
			puts(" H                        Show help");
			puts(" L                        Toggle lock grid relative to samples");
			puts(" LEFT                     Move left");
			puts(" <CTL>LEFT                Move left 1 sample");
			puts(" <SHIFT>LEFT              Page left");
			puts(" LEFT-MOUSE-CLICK         Set yellow cursor");
			puts(" Q                        Hide window");
			puts(" RIGHT                    Move right");
			puts(" <CTL>RIGHT               Move right 1 sample");
			puts(" <SHIFT>RIGHT             Page right");
			puts(" RIGHT-MOUSE-CLICK        Set purple cursor");
			puts(" UP                       Zoom out");
			puts("");
			puts("Use client window 'data help' for more plot commands\n");
			break;

		case Qt::Key_L:
			GridLocked= !GridLocked;
			break;

		case Qt::Key_Q:
			this->hide();
			break;

		default:
			QWidget::keyPressEvent(event);
			return;
			break;
	}

	this->update();
}
