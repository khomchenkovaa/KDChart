/****************************************************************************
**
** This file is part of the KD Chart library.
**
** SPDX-FileCopyrightText: 2001 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
**
** SPDX-License-Identifier: MIT
**
****************************************************************************/

#include "KDChartAbstractCoordinatePlane.h"
#include "KDChartAbstractCoordinatePlane_p.h"

#include "KDChartChart.h"
#include "KDChartGridAttributes.h"

#include <KDABLibFakes>

#include <QGridLayout>
#include <QMouseEvent>
#include <QRubberBand>
#include <QtCore/qmath.h>

using namespace KDChart;

#define d d_func()

AbstractCoordinatePlane::Private::Private()
    : AbstractArea::Private()
{
    // this block left empty intentionally
}

AbstractCoordinatePlane::AbstractCoordinatePlane(KDChart::Chart *parent)
    : AbstractArea(new Private())
{
    d->parent = parent;
    d->init();
}

AbstractCoordinatePlane::~AbstractCoordinatePlane()
{
    Q_EMIT destroyedCoordinatePlane(this);
}

void AbstractCoordinatePlane::init()
{
    d->initialize(); // virtual method to init the correct grid: cartesian, polar, ...
    connect(this, &AbstractCoordinatePlane::internal_geometryChanged,
            this, &AbstractCoordinatePlane::geometryChanged,
            Qt::QueuedConnection);
}

void AbstractCoordinatePlane::addDiagram(AbstractDiagram *diagram)
{
    // diagrams are invisible and paint through their paint() method
    diagram->hide();

    d->diagrams.append(diagram);
    diagram->setParent(d->parent);
    diagram->setCoordinatePlane(this);
    layoutDiagrams();
    layoutPlanes(); // there might be new axes, etc
    connect(diagram, &AbstractDiagram::modelsChanged, this, &AbstractCoordinatePlane::layoutPlanes);
    connect(diagram, &AbstractDiagram::modelDataChanged, this, &AbstractCoordinatePlane::update);
    connect(diagram, &AbstractDiagram::modelDataChanged, this, &AbstractCoordinatePlane::relayout);
    connect(this, &AbstractCoordinatePlane::boundariesChanged, diagram, &AbstractDiagram::boundariesChanged);

    update();
    Q_EMIT boundariesChanged();
}

/*virtual*/
void AbstractCoordinatePlane::replaceDiagram(AbstractDiagram *diagram, AbstractDiagram *oldDiagram_)
{
    if (diagram && oldDiagram_ != diagram) {
        AbstractDiagram *oldDiagram = oldDiagram_;
        if (d->diagrams.count()) {
            if (!oldDiagram) {
                oldDiagram = d->diagrams.first();
                if (oldDiagram == diagram)
                    return;
            }
            takeDiagram(oldDiagram);
        }
        delete oldDiagram;
        addDiagram(diagram);
        layoutDiagrams();
        layoutPlanes(); // there might be new axes, etc
        update();
    }
}

/*virtual*/
void AbstractCoordinatePlane::takeDiagram(AbstractDiagram *diagram)
{
    const int idx = d->diagrams.indexOf(diagram);
    if (idx != -1) {
        d->diagrams.removeAt(idx);
        diagram->setParent(nullptr);
        diagram->setCoordinatePlane(nullptr);
        disconnect(diagram, &AbstractDiagram::modelsChanged, this, &AbstractCoordinatePlane::layoutPlanes);
        disconnect(diagram, &AbstractDiagram::modelDataChanged, this, &AbstractCoordinatePlane::update);
        disconnect(diagram, &AbstractDiagram::modelDataChanged, this, &AbstractCoordinatePlane::relayout);
        layoutDiagrams();
        update();
    }
}

AbstractDiagram *AbstractCoordinatePlane::diagram()
{
    if (d->diagrams.isEmpty()) {
        return nullptr;
    } else {
        return d->diagrams.first();
    }
}

AbstractDiagramList AbstractCoordinatePlane::diagrams()
{
    return d->diagrams;
}

ConstAbstractDiagramList AbstractCoordinatePlane::diagrams() const
{
    ConstAbstractDiagramList list;
#ifndef QT_NO_STL
    qCopy(d->diagrams.begin(), d->diagrams.end(), std::back_inserter(list));
#else
    for (AbstractDiagram *a : d->diagrams)
        list.push_back(a);
#endif
    return list;
}

void KDChart::AbstractCoordinatePlane::setGlobalGridAttributes(const GridAttributes &a)
{
    d->gridAttributes = a;
    update();
}

GridAttributes KDChart::AbstractCoordinatePlane::globalGridAttributes() const
{
    return d->gridAttributes;
}

KDChart::DataDimensionsList KDChart::AbstractCoordinatePlane::gridDimensionsList()
{
    return d->grid->updateData(this);
}

void KDChart::AbstractCoordinatePlane::setGridNeedsRecalculate()
{
    d->grid->setNeedRecalculate();
}

void KDChart::AbstractCoordinatePlane::setReferenceCoordinatePlane(AbstractCoordinatePlane *plane)
{
    d->referenceCoordinatePlane = plane;
}

AbstractCoordinatePlane *KDChart::AbstractCoordinatePlane::referenceCoordinatePlane() const
{
    return d->referenceCoordinatePlane;
}

void KDChart::AbstractCoordinatePlane::setParent(KDChart::Chart *parent)
{
    d->parent = parent;
}

const KDChart::Chart *KDChart::AbstractCoordinatePlane::parent() const
{
    return d->parent;
}

KDChart::Chart *KDChart::AbstractCoordinatePlane::parent()
{
    return d->parent;
}

/* pure virtual in QLayoutItem */
bool KDChart::AbstractCoordinatePlane::isEmpty() const
{
    return false; // never empty!
    // coordinate planes with no associated diagrams
    // are showing a default grid of ()1..10, 1..10) stepWidth 1
}
/* pure virtual in QLayoutItem */
Qt::Orientations KDChart::AbstractCoordinatePlane::expandingDirections() const
{
    return Qt::Vertical | Qt::Horizontal;
}
/* pure virtual in QLayoutItem */
QSize KDChart::AbstractCoordinatePlane::maximumSize() const
{
    // No maximum size set. Especially not parent()->size(), we are not layouting
    // to the parent widget's size when using Chart::paint()!
    return QSize(QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX);
}
/* pure virtual in QLayoutItem */
QSize KDChart::AbstractCoordinatePlane::minimumSize() const
{
    return QSize(60, 60); // this default can be overwritten by derived classes
}
/* pure virtual in QLayoutItem */
QSize KDChart::AbstractCoordinatePlane::sizeHint() const
{
    // we return our maxiumu (which is the full size of the Chart)
    // even if we know the plane will be smaller
    return maximumSize();
}
/* pure virtual in QLayoutItem */
void KDChart::AbstractCoordinatePlane::setGeometry(const QRect &r)
{
    if (d->geometry != r) {
        // inform the outside word by Signal geometryChanged()
        // via a queued connection to internal_geometryChanged()
        Q_EMIT internal_geometryChanged(d->geometry, r);

        d->geometry = r;
        // Note: We do *not* call update() here
        //       because it would invoke KDChart::update() recursively.
    }
}
/* pure virtual in QLayoutItem */
QRect KDChart::AbstractCoordinatePlane::geometry() const
{
    return d->geometry;
}

void KDChart::AbstractCoordinatePlane::update()
{
    // qDebug("KDChart::AbstractCoordinatePlane::update() called");
    Q_EMIT needUpdate();
}

void KDChart::AbstractCoordinatePlane::relayout()
{
    // qDebug("KDChart::AbstractCoordinatePlane::relayout() called");
    Q_EMIT needRelayout();
}

void KDChart::AbstractCoordinatePlane::layoutPlanes()
{
    // qDebug("KDChart::AbstractCoordinatePlane::relayout() called");
    Q_EMIT needLayoutPlanes();
}

void KDChart::AbstractCoordinatePlane::setRubberBandZoomingEnabled(bool enable)
{
    d->enableRubberBandZooming = enable;

    if (!enable && d->rubberBand != nullptr) {
        delete d->rubberBand;
        d->rubberBand = nullptr;
    }
}

bool KDChart::AbstractCoordinatePlane::isRubberBandZoomingEnabled() const
{
    return d->enableRubberBandZooming;
}

void KDChart::AbstractCoordinatePlane::setCornerSpacersEnabled(bool enable)
{
    if (d->enableCornerSpacers == enable)
        return;

    d->enableCornerSpacers = enable;
    Q_EMIT needRelayout();
}

bool KDChart::AbstractCoordinatePlane::isCornerSpacersEnabled() const
{
    return d->enableCornerSpacers;
}

void KDChart::AbstractCoordinatePlane::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (d->enableRubberBandZooming && d->rubberBand == nullptr)
            d->rubberBand = new QRubberBand(QRubberBand::Rectangle, qobject_cast<QWidget *>(parent()));

        if (d->rubberBand != nullptr) {
            d->rubberBandOrigin = event->pos();
            d->rubberBand->setGeometry(QRect(event->pos(), QSize()));
            d->rubberBand->show();

            event->accept();
        }
    } else if (event->button() == Qt::RightButton) {
        if (d->enableRubberBandZooming && !d->rubberBandZoomConfigHistory.isEmpty()) {
            // restore the last config from the stack
            ZoomParameters config = d->rubberBandZoomConfigHistory.pop();
            setZoomFactorX(config.xFactor);
            setZoomFactorY(config.yFactor);
            setZoomCenter(config.center());

            QWidget *const p = qobject_cast<QWidget *>(parent());
            if (p != nullptr)
                p->update();

            event->accept();
        }
    }

    for (AbstractDiagram *a : qAsConst(d->diagrams)) {
        a->mousePressEvent(event);
    }
}

void KDChart::AbstractCoordinatePlane::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        // otherwise the second click gets lost
        // which is pretty annoying when zooming out fast
        mousePressEvent(event);
    }
    for (AbstractDiagram *a : qAsConst(d->diagrams)) {
        a->mouseDoubleClickEvent(event);
    }
}

void KDChart::AbstractCoordinatePlane::mouseReleaseEvent(QMouseEvent *event)
{
    if (d->rubberBand != nullptr) {
        // save the old config on the stack
        d->rubberBandZoomConfigHistory.push(ZoomParameters(zoomFactorX(), zoomFactorY(), zoomCenter()));

        // this is the height/width of the rubber band in pixel space
        const auto rubberWidth = static_cast<qreal>(d->rubberBand->width());
        const auto rubberHeight = static_cast<qreal>(d->rubberBand->height());

        if (rubberWidth > 0.0 && rubberHeight > 0.0) {
            // this is the center of the rubber band in pixel space
            const qreal centerX = qFloor(d->rubberBand->geometry().width() / 2.0 + d->rubberBand->geometry().x());
            const qreal centerY = qCeil(d->rubberBand->geometry().height() / 2.0 + d->rubberBand->geometry().y());

            const qreal rubberCenterX = static_cast<qreal>(centerX - geometry().x());
            const qreal rubberCenterY = static_cast<qreal>(centerY - geometry().y());

            // this is the height/width of the plane in pixel space
            const qreal myWidth = static_cast<qreal>(geometry().width());
            const qreal myHeight = static_cast<qreal>(geometry().height());

            // this describes the new center of zooming, relative to the plane pixel space
            const qreal newCenterX = rubberCenterX / myWidth / zoomFactorX() + zoomCenter().x() - 0.5 / zoomFactorX();
            const qreal newCenterY = rubberCenterY / myHeight / zoomFactorY() + zoomCenter().y() - 0.5 / zoomFactorY();

            // this will be the new zoom factor
            const qreal newZoomFactorX = zoomFactorX() * myWidth / rubberWidth;
            const qreal newZoomFactorY = zoomFactorY() * myHeight / rubberHeight;

            // and this the new center
            const QPointF newZoomCenter(newCenterX, newCenterY);

            setZoomFactorX(newZoomFactorX);
            setZoomFactorY(newZoomFactorY);
            setZoomCenter(newZoomCenter);
        }

        d->rubberBand->parentWidget()->update();
        delete d->rubberBand;
        d->rubberBand = nullptr;

        event->accept();
    }

    for (AbstractDiagram *a : qAsConst(d->diagrams)) {
        a->mouseReleaseEvent(event);
    }
}

void KDChart::AbstractCoordinatePlane::mouseMoveEvent(QMouseEvent *event)
{
    if (d->rubberBand != nullptr) {
        const QRect normalized = QRect(d->rubberBandOrigin, event->pos()).normalized();
        d->rubberBand->setGeometry(normalized & geometry());

        event->accept();
    }

    for (AbstractDiagram *a : qAsConst(d->diagrams)) {
        a->mouseMoveEvent(event);
    }
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && defined(Q_COMPILER_MANGLES_RETURN_TYPE)
const
#endif
    bool
    KDChart::AbstractCoordinatePlane::isVisiblePoint(const QPointF &point) const
{
    return d->isVisiblePoint(this, point);
}

AbstractCoordinatePlane *KDChart::AbstractCoordinatePlane::sharedAxisMasterPlane(QPainter *p)
{
    Q_UNUSED(p);
    return this;
}

#if !defined(QT_NO_DEBUG_STREAM)
#include "KDChartEnums.h"

QDebug KDChart::operator<<(QDebug stream, const DataDimension &r)
{
    stream << "DataDimension("
           << " start=" << r.start
           << " end=" << r.end
           << " sequence=" << KDChartEnums::granularitySequenceToString(r.sequence)
           << " isCalculated=" << r.isCalculated
           << " calcMode=" << (r.calcMode == AbstractCoordinatePlane::Logarithmic ? "Logarithmic" : "Linear")
           << " stepWidth=" << r.stepWidth
           << " subStepWidth=" << r.subStepWidth
           << " )";
    return stream;
}
#endif

#undef d
