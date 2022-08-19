/****************************************************************************
**
** This file is part of the KD Chart library.
**
** SPDX-FileCopyrightText: 2001-2022 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
**
** SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDAB-KDChart OR LicenseRef-KDAB-KDChart-US
**
** Licensees holding valid commercial KD Chart licenses may use this file in
** accordance with the KD Chart Commercial License Agreement provided with
** the Software.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
****************************************************************************/

#include "KDChartMeasure.h"

#include <QWidget>

#include <KDChartAbstractArea.h>
#include <KDChartBackgroundAttributes.h>
#include <KDChartCartesianCoordinatePlane.h>
#include <KDChartFrameAttributes.h>
#include <KDChartTextAttributes.h>

#include <KDABLibFakes>

namespace KDChart {

Measure::Measure()
{
    // this block left empty intentionally
}

Measure::Measure(qreal value,
                 KDChartEnums::MeasureCalculationMode mode,
                 KDChartEnums::MeasureOrientation orientation)
    : mValue(value)
    , mMode(mode)
    , mArea(nullptr)
    , mOrientation(orientation)
{
    // this block left empty intentionally
}

Measure::Measure(const Measure &r)
    : mValue(r.value())
    , mMode(r.calculationMode())
    , mArea(r.referenceArea())
    , mOrientation(r.referenceOrientation())
{
    // this block left empty intentionally
}

Measure &Measure::operator=(const Measure &r)
{
    if (this != &r) {
        mValue = r.value();
        mMode = r.calculationMode();
        mArea = r.referenceArea();
        mOrientation = r.referenceOrientation();
    }

    return *this;
}

qreal Measure::calculatedValue(const QSizeF &autoSize,
                               KDChartEnums::MeasureOrientation autoOrientation) const
{
    if (mMode == KDChartEnums::MeasureCalculationModeAbsolute) {
        return mValue;
    } else {
        qreal value = 0.0;
        const QObject theAutoArea;
        const QObject *autoArea = &theAutoArea;
        const QObject *area = mArea ? mArea : autoArea;
        KDChartEnums::MeasureOrientation orientation = mOrientation;
        switch (mMode) {
        case KDChartEnums::MeasureCalculationModeAuto:
            area = autoArea;
            orientation = autoOrientation;
            break;
        case KDChartEnums::MeasureCalculationModeAutoArea:
            area = autoArea;
            break;
        case KDChartEnums::MeasureCalculationModeAutoOrientation:
            orientation = autoOrientation;
            break;
        case KDChartEnums::MeasureCalculationModeAbsolute: // fall through intended
        case KDChartEnums::MeasureCalculationModeRelative:
            break;
        }
        if (area) {
            QSizeF size;
            if (area == autoArea)
                size = autoSize;
            else
                size = sizeOfArea(area);
            // qDebug() << ( area == autoArea ) << "size" << size;
            qreal referenceValue = 0;
            switch (orientation) {
            case KDChartEnums::MeasureOrientationAuto: // fall through intended
            case KDChartEnums::MeasureOrientationMinimum:
                referenceValue = qMin(size.width(), size.height());
                break;
            case KDChartEnums::MeasureOrientationMaximum:
                referenceValue = qMax(size.width(), size.height());
                break;
            case KDChartEnums::MeasureOrientationHorizontal:
                referenceValue = size.width();
                break;
            case KDChartEnums::MeasureOrientationVertical:
                referenceValue = size.height();
                break;
            }
            value = mValue / 1000.0 * referenceValue;
        }
        return value;
    }
}

qreal Measure::calculatedValue(const QObject *autoArea,
                               KDChartEnums::MeasureOrientation autoOrientation) const
{
    return calculatedValue(sizeOfArea(autoArea), autoOrientation);
}

const QSizeF Measure::sizeOfArea(const QObject *area) const
{
    QSizeF size;
    const auto *plane = dynamic_cast<const CartesianCoordinatePlane *>(area);
    if (false) {
        size = plane->visibleDiagramArea().size();
    } else {
        const auto *kdcArea = dynamic_cast<const AbstractArea *>(area);
        if (kdcArea) {
            size = kdcArea->geometry().size();
            // qDebug() << "Measure::sizeOfArea() found kdcArea with size" << size;
        } else {
            const auto *widget = dynamic_cast<const QWidget *>(area);
            if (widget) {
                /* ATTENTION: Using the layout does not work: The Legend will never get the right size then!
                const QLayout * layout = widget->layout();
                if ( layout ) {
                    size = layout->geometry().size();
                    //qDebug() << "Measure::sizeOfArea() found widget with layout size" << size;
                } else*/
                {
                    size = widget->geometry().size();
                    // qDebug() << "Measure::sizeOfArea() found widget with size" << size;
                }
            } else if (mMode != KDChartEnums::MeasureCalculationModeAbsolute) {
                size = QSizeF(1.0, 1.0);
                // qDebug("Measure::sizeOfArea() got no valid area.");
            }
        }
    }
    const QPair<qreal, qreal> factors = GlobalMeasureScaling::instance()->currentFactors();
    return QSizeF(size.width() * factors.first, size.height() * factors.second);
}

bool Measure::operator==(const Measure &r) const
{
    return (mValue == r.value() && mMode == r.calculationMode() && mArea == r.referenceArea() && mOrientation == r.referenceOrientation());
}

GlobalMeasureScaling::GlobalMeasureScaling()
{
    mFactors.push(qMakePair(qreal(1.0), qreal(1.0)));
}

GlobalMeasureScaling::~GlobalMeasureScaling()
{
    // this space left empty intentionally
}

GlobalMeasureScaling *GlobalMeasureScaling::instance()
{
    static GlobalMeasureScaling instance;
    return &instance;
}

void GlobalMeasureScaling::setFactors(qreal factorX, qreal factorY)
{
    instance()->mFactors.push(qMakePair(factorX, factorY));
}

void GlobalMeasureScaling::resetFactors()
{
    // never remove the initial (1.0. 1.0) setting
    if (instance()->mFactors.count() > 1)
        instance()->mFactors.pop();
}

const QPair<qreal, qreal> GlobalMeasureScaling::currentFactors()
{
    return instance()->mFactors.top();
}

void GlobalMeasureScaling::setPaintDevice(QPaintDevice *paintDevice)
{
    instance()->m_paintDevice = paintDevice;
}

QPaintDevice *GlobalMeasureScaling::paintDevice()
{
    return instance()->m_paintDevice;
}
}

#if !defined(QT_NO_DEBUG_STREAM)
QDebug operator<<(QDebug dbg, const KDChart::Measure &m)
{
    dbg << "KDChart::Measure("
        << "value=" << m.value()
        << "calculationmode=" << m.calculationMode()
        << "referencearea=" << m.referenceArea()
        << "referenceorientation=" << m.referenceOrientation()
        << ")";
    return dbg;
}
#endif /* QT_NO_DEBUG_STREAM */
