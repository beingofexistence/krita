/*
 *  Copyright (c) 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_resources_snapshot.h"

#include <KoColor.h>
#include <KoAbstractGradient.h>
#include "kis_painter.h"
#include "kis_paintop_preset.h"
#include "kis_paintop_settings.h"
#include "kis_pattern.h"
#include "kis_canvas_resource_provider.h"
#include "filter/kis_filter_configuration.h"
#include "kis_image.h"
#include "kis_paint_device.h"
#include "kis_paint_layer.h"


struct KisResourcesSnapshot::Private {
    Private() : currentPattern(0), currentGradient(0),
                currentGenerator(0), compositeOp(0)
    {
    }

    KisImageWSP image;
    KoColor currentFgColor;
    KoColor currentBgColor;
    KisPattern *currentPattern;
    KoAbstractGradient *currentGradient;
    KisPaintOpPresetSP currentPaintOpPreset;
    KisNodeSP currentNode;
    qreal currentExposure;
    KisFilterConfiguration *currentGenerator;

    QPointF axisCenter;
    bool mirrorMaskHorizontal;
    bool mirrorMaskVertical;

    quint8 opacity;
    const KoCompositeOp *compositeOp;
};

KisResourcesSnapshot::KisResourcesSnapshot(KisImageWSP image, KoResourceManager *resourceManager)
    : m_d(new Private())
{
    m_d->image = image;
    m_d->currentFgColor = resourceManager->resource(KoCanvasResource::ForegroundColor).value<KoColor>();
    m_d->currentBgColor = resourceManager->resource(KoCanvasResource::BackgroundColor).value<KoColor>();
    m_d->currentPattern = static_cast<KisPattern*>(resourceManager->resource(KisCanvasResourceProvider::CurrentPattern).value<void*>());
    m_d->currentGradient = static_cast<KoAbstractGradient*>(resourceManager->resource(KisCanvasResourceProvider::CurrentGradient).value<void*>());
    m_d->currentPaintOpPreset = resourceManager->resource(KisCanvasResourceProvider::CurrentPaintOpPreset).value<KisPaintOpPresetSP>();
    m_d->currentNode = resourceManager->resource(KisCanvasResourceProvider::CurrentKritaNode).value<KisNodeSP>();
    m_d->currentExposure = resourceManager->resource(KisCanvasResourceProvider::HdrExposure).toDouble();
    m_d->currentGenerator = static_cast<KisFilterConfiguration*>(resourceManager->resource(KisCanvasResourceProvider::CurrentGeneratorConfiguration).value<void*>());

    m_d->axisCenter = resourceManager->resource(KisCanvasResourceProvider::MirrorAxisCenter).toPointF();
    if (m_d->axisCenter.isNull()){
        m_d->axisCenter = QPointF(0.5 * image->width(), 0.5 * image->height());
    }

    m_d->mirrorMaskHorizontal = resourceManager->resource(KisCanvasResourceProvider::MirrorHorizontal).toBool();
    m_d->mirrorMaskVertical = resourceManager->resource(KisCanvasResourceProvider::MirrorVertical).toBool();


    qreal normOpacity = resourceManager->resource(KisCanvasResourceProvider::Opacity).toDouble();
    m_d->opacity = quint8(normOpacity * OPACITY_OPAQUE_U8);


    QString compositeOpId = resourceManager->resource(KisCanvasResourceProvider::CurrentCompositeOp).toString();
    KisPaintDeviceSP device;

    if(m_d->currentNode && (device = m_d->currentNode->paintDevice())) {
        m_d->compositeOp = device->colorSpace()->compositeOp(compositeOpId);
        if(!m_d->compositeOp) {
            m_d->compositeOp = device->colorSpace()->compositeOp(COMPOSITE_OVER);
        }
    }
}

KisResourcesSnapshot::~KisResourcesSnapshot()
{
    delete m_d;
}

void KisResourcesSnapshot::setupPainter(KisPainter* painter)
{
    painter->setBounds(m_d->image->bounds());
    painter->setPaintColor(m_d->currentFgColor);
    painter->setBackgroundColor(m_d->currentBgColor);
    painter->setGenerator(m_d->currentGenerator);
    painter->setPattern(m_d->currentPattern);
    painter->setGradient(m_d->currentGradient);
    painter->setPaintOpPreset(m_d->currentPaintOpPreset, m_d->image);

    KisPaintLayer *paintLayer;
    if (paintLayer = dynamic_cast<KisPaintLayer*>(m_d->currentNode.data())) {
        painter->setChannelFlags(paintLayer->channelLockFlags());
    }

    painter->setOpacity(m_d->opacity);
    painter->setCompositeOp(m_d->compositeOp);
    painter->setMirrorInformation(m_d->axisCenter, m_d->mirrorMaskHorizontal, m_d->mirrorMaskVertical);
}

KisImageWSP KisResourcesSnapshot::image() const
{
    return m_d->image;
}

KisNodeSP KisResourcesSnapshot::currentNode() const
{
    return m_d->currentNode;
}

quint8 KisResourcesSnapshot::opacity() const
{
    return m_d->opacity;
}

const KoCompositeOp* KisResourcesSnapshot::compositeOp() const
{
    return m_d->compositeOp;
}
