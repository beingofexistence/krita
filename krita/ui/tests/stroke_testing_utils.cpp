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

#include "stroke_testing_utils.h"

#include <QtTest/QtTest>

#include <QDir>
#include <KoColor.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include "kis_painter.h"
#include "kis_paintop_preset.h"
#include "kis_pattern.h"
#include "kis_canvas_resource_provider.h"
#include "kis_image.h"
#include "kis_paint_device.h"
#include "kis_paint_layer.h"
#include "kis_dumb_undo_adapter.h"



KisImageSP utils::createImage(KisUndoAdapter *undoAdapter) {
    QRect imageRect(0,0,500,500);

    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(undoAdapter, imageRect.width(), imageRect.height(), cs, "stroke test");

    KisPaintLayerSP paintLayer1 = new KisPaintLayer(image, "paint1", OPACITY_OPAQUE_U8);

    image->lock();
    image->addNode(paintLayer1);
    image->unlock();
    return image;
}

KoResourceManager* utils::createResourceManager(KisImageWSP image,
                                                KisNodeSP node,
                                                const QString &presetFileName)
{
    KoResourceManager *manager = new KoResourceManager();

    QVariant i;

    i.setValue(KoColor(Qt::black, image->colorSpace()));
    manager->setResource(KoCanvasResource::ForegroundColor, i);

    i.setValue(KoColor(Qt::white, image->colorSpace()));
    manager->setResource(KoCanvasResource::BackgroundColor, i);

    i.setValue(static_cast<void*>(0));
    manager->setResource(KisCanvasResourceProvider::CurrentPattern, i);
    manager->setResource(KisCanvasResourceProvider::CurrentGradient, i);
    manager->setResource(KisCanvasResourceProvider::CurrentGeneratorConfiguration, i);

    if(!node) {
        node = image->root();

        while(node && !dynamic_cast<KisPaintLayer*>(node.data())) {
            node = node->firstChild();
        }

        Q_ASSERT(node && dynamic_cast<KisPaintLayer*>(node.data()));
    }

    i.setValue(node);
    manager->setResource(KisCanvasResourceProvider::CurrentKritaNode, i);

    QString dataPath = QString(FILES_DATA_DIR) + QDir::separator();
    KisPaintOpPresetSP preset = new KisPaintOpPreset(dataPath + presetFileName);
    preset->load();

    i.setValue(preset);
    manager->setResource(KisCanvasResourceProvider::CurrentPaintOpPreset, i);

    i.setValue(COMPOSITE_OVER);
    manager->setResource(KisCanvasResourceProvider::CurrentCompositeOp, i);

    i.setValue(false);
    manager->setResource(KisCanvasResourceProvider::MirrorHorizontal, i);

    i.setValue(false);
    manager->setResource(KisCanvasResourceProvider::MirrorVertical, i);

    i.setValue(1.0);
    manager->setResource(KisCanvasResourceProvider::Opacity, i);

    i.setValue(1.0);
    manager->setResource(KisCanvasResourceProvider::HdrExposure, i);

    i.setValue(QPoint());
    manager->setResource(KisCanvasResourceProvider::MirrorAxisCenter, i);

    return manager;
}


utils::StrokeTester::StrokeTester(const QString &name)
    : m_name(name)
{
}

utils::StrokeTester::~StrokeTester()
{
}

void utils::StrokeTester::test()
{
    testOneStroke(false, false);
    testOneStroke(false, true);
    testOneStroke(true, false);
    testOneStroke(true, true);
}

void utils::StrokeTester::testOneStroke(bool cancelled,
                                        bool indirectPainting)
{
    QString filename;
    QImage image;

    filename = formatFilename(m_name, cancelled, indirectPainting);
    qDebug() << "Testing reference:" << filename;
    image = doStroke(cancelled, indirectPainting);
    image.save(QString(FILES_OUTPUT_DIR) + QDir::separator() + filename);

    QImage refImage;
    refImage.load(QString(FILES_DATA_DIR) + QDir::separator() + filename);

    QCOMPARE(image, refImage);
}

QString utils::StrokeTester::formatFilename(const QString &baseName,
                                            bool cancelled,
                                            bool indirectPainting)
{
    QString result = baseName;
    result += indirectPainting ? "_indirect" : "_incremental";
    result += cancelled ? "_cancelled" : "_finished";
    result += ".png";
    return result;
}

QImage utils::StrokeTester::doStroke(bool cancelled, bool indirectPainting)
{
    KisUndoAdapter *undoAdapter = new KisDumbUndoAdapter();
    KisImageSP image = utils::createImage(undoAdapter);
    KoResourceManager *manager = utils::createResourceManager(image);

    KisPainter *painter = new KisPainter();
    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image, manager);

    KisStrokeStrategy *stroke = createStroke(indirectPainting, resources, painter);
    image->startStroke(stroke);
    addPaintingJobs(image, resources, painter);

    if(!cancelled) {
        image->endStroke();
    }
    else {
        image->cancelStroke();
    }

    QTest::qSleep(2000);

    KisPaintDeviceSP device = resources->currentNode()->paintDevice();
    QImage resultImage = device->convertToQImage(0, 0, 0, image->width(), image->height());

    image = 0;
    delete manager;
    delete undoAdapter;
    return resultImage;
}
