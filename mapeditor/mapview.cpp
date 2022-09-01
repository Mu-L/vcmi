#include "StdInc.h"
#include "mapview.h"
#include "mainwindow.h"
#include <QGraphicsSceneMouseEvent>

MapView::MapView(QWidget *parent):
	QGraphicsView(parent),
	selectionTool(MapView::SelectionTool::None)
{
}

void MapView::setMain(MainWindow * m)
{
	main = m;
}

void MapView::mouseMoveEvent(QMouseEvent *mouseEvent)
{
	this->update();

	auto * sc = static_cast<MapScene*>(scene());
	if(!sc)
		return;

	auto pos = mapToScene(mouseEvent->pos()); //TODO: do we need to check size?
	int3 tile(pos.x() / 32, pos.y() / 32, sc->level);

	if(tile == tilePrev) //do not redraw
		return;

	tilePrev = tile;

	//main->setStatusMessage(QString("x: %1 y: %2").arg(QString::number(pos.x()), QString::number(pos.y())));

	switch(selectionTool)
	{
	case MapView::SelectionTool::Brush:
		if(pressedOnSelected)
			sc->selectionTerrainView.erase(tile);
		else
			sc->selectionTerrainView.select(tile);
		sc->selectionTerrainView.draw();
		break;

	case MapView::SelectionTool::Area:
		sc->selectionTerrainView.clear();
		for(int j = std::min(tile.y, tileStart.y); j < std::max(tile.y, tileStart.y); ++j)
		{
			for(int i = std::min(tile.x, tileStart.x); i < std::max(tile.x, tileStart.x); ++i)
			{
				sc->selectionTerrainView.select(int3(i, j, sc->level));
			}
		}
		sc->selectionTerrainView.draw();
		break;

	case MapView::SelectionTool::None:
		auto sh = tile - tileStart;
		sc->selectionObjectsView.shift = QPoint(sh.x, sh.y);
		sc->selectionObjectsView.draw();
		break;
	}
}

void MapView::mousePressEvent(QMouseEvent *event)
{
	this->update();

	auto * sc = static_cast<MapScene*>(scene());
	if(!sc)
		return;

	mouseStart = mapToScene(event->pos());
	tileStart = tilePrev = int3(mouseStart.x() / 32, mouseStart.y() / 32, sc->level);

	if(sc->selectionTerrainView.selection().count(tileStart))
		pressedOnSelected = true;
	else
		pressedOnSelected = false;

	switch(selectionTool)
	{
	case MapView::SelectionTool::Brush:
		sc->selectionObjectsView.clear();
		sc->selectionObjectsView.draw();
		if(pressedOnSelected)
			sc->selectionTerrainView.erase(tileStart);
		else
			sc->selectionTerrainView.select(tileStart);
		sc->selectionTerrainView.draw();
		break;

	case MapView::SelectionTool::Area:
		sc->selectionTerrainView.clear();
		sc->selectionTerrainView.draw();
		sc->selectionObjectsView.clear();
		sc->selectionObjectsView.draw();
		break;

	case MapView::SelectionTool::None:
		sc->selectionTerrainView.clear();
		sc->selectionTerrainView.draw();
		sc->selectionObjectsView.clear();
		sc->selectionObjectsView.selectObjectAt(tileStart.x, tileStart.y);
		sc->selectionObjectsView.draw();
		break;
	}

	//main->setStatusMessage(QString("x: %1 y: %2").arg(QString::number(event->pos().x()), QString::number(event->pos().y())));
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
	this->update();

	auto * sc = static_cast<MapScene*>(scene());
	if(!sc)
		return;

	switch(selectionTool)
	{
	case MapView::SelectionTool::None:
		sc->selectionObjectsView.shift = QPoint(0, 0);
		sc->selectionObjectsView.draw();
		break;
	}
}

MapScene::MapScene(MainWindow *parent, int l):
	QGraphicsScene(parent),
	gridView(parent, this),
	passabilityView(parent, this),
	selectionTerrainView(parent, this),
	terrainView(parent, this),
	objectsView(parent, this),
	selectionObjectsView(parent, this),
	main(parent),
	level(l)
{
}

void MapScene::updateViews()
{
	//sequence is important because it defines rendering order
	terrainView.update();
	objectsView.update();
	gridView.update();
	passabilityView.update();
	selectionTerrainView.update();
	selectionObjectsView.update();

	terrainView.show(true);
	objectsView.show(true);
	selectionTerrainView.show(true);
	selectionObjectsView.show(true);
}

BasicView::BasicView(MainWindow * m, MapScene * s): main(m), scene(s)
{
	if(main->getMap())
		update();
}

void BasicView::show(bool show)
{
	if(isShown == show)
		return;

	if(show)
	{
		if(pixmap)
		{
			if(item)
				item->setPixmap(*pixmap);
			else
				item.reset(scene->addPixmap(*pixmap));
		}
		else
		{
			if(item)
				item->setPixmap(emptyPixmap);
			else
				item.reset(scene->addPixmap(emptyPixmap));
		}
	}
	else
	{
		item->setPixmap(emptyPixmap);
	}

	isShown = show;
}

void BasicView::redraw()
{
	if(item)
	{
		if(pixmap && isShown)
			item->setPixmap(*pixmap);
		else
			item->setPixmap(emptyPixmap);
	}
	else
	{
		if(pixmap && isShown)
			item.reset(scene->addPixmap(*pixmap));
		else
			item.reset(scene->addPixmap(emptyPixmap));
	}
}

GridView::GridView(MainWindow * m, MapScene * s): BasicView(m, s)
{
}

void GridView::update()
{
	auto map = main->getMap();
	if(!map)
		return;

	pixmap.reset(new QPixmap(map->width * 32, map->height * 32));
	pixmap->fill(QColor(0, 0, 0, 0));
	QPainter painter(pixmap.get());
	painter.setPen(QColor(0, 0, 0, 190));

	for(int j = 0; j < map->height; ++j)
	{
		painter.drawLine(0, j * 32, map->width * 32 - 1, j * 32);
	}
	for(int i = 0; i < map->width; ++i)
	{
		painter.drawLine(i * 32, 0, i * 32, map->height * 32 - 1);
	}

	redraw();
}

PassabilityView::PassabilityView(MainWindow * m, MapScene * s): BasicView(m, s)
{
}

void PassabilityView::update()
{
	auto map = main->getMap();
	if(!map)
		return;

	pixmap.reset(new QPixmap(map->width * 32, map->height * 32));
	pixmap->fill(QColor(0, 0, 0, 0));

	if(scene->level == 0 || map->twoLevel)
	{
		QPainter painter(pixmap.get());
		for(int j = 0; j < map->height; ++j)
		{
			for(int i = 0; i < map->width; ++i)
			{
				auto tl = map->getTile(int3(i, j, scene->level));
				if(tl.blocked || tl.visitable)
				{
					painter.fillRect(i * 32, j * 32, 31, 31, tl.visitable ? QColor(200, 200, 0, 64) : QColor(255, 0, 0, 64));
				}
			}
		}
	}

	redraw();
}

SelectionTerrainView::SelectionTerrainView(MainWindow * m, MapScene * s): BasicView(m, s)
{
}

void SelectionTerrainView::update()
{
	auto map = main->getMap();
	if(!map)
		return;

	area.clear();
	areaAdd.clear();
	areaErase.clear();

	pixmap.reset(new QPixmap(map->width * 32, map->height * 32));
	pixmap->fill(QColor(0, 0, 0, 0));

	redraw();
}

void SelectionTerrainView::draw()
{
	if(!pixmap)
		return;

	QPainter painter(pixmap.get());
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	for(auto & t : areaAdd)
	{
		painter.fillRect(t.x * 32, t.y * 32, 31, 31, QColor(128, 128, 128, 96));
	}
	for(auto & t : areaErase)
	{
		painter.fillRect(t.x * 32, t.y * 32, 31, 31, QColor(0, 0, 0, 0));
	}

	areaAdd.clear();
	areaErase.clear();

	redraw();
}

void SelectionTerrainView::select(const int3 & tile)
{
	if(!area.count(tile))
	{
		area.insert(tile);
		areaAdd.insert(tile);
		areaErase.erase(tile);
	}
}

void SelectionTerrainView::erase(const int3 & tile)
{
	if(area.count(tile))
	{
		area.erase(tile);
		areaErase.insert(tile);
		areaAdd.erase(tile);
	}
}

void SelectionTerrainView::clear()
{
	areaErase = area;
	areaAdd.clear();
	area.clear();
}

const std::set<int3> & SelectionTerrainView::selection() const
{
	return area;
}

TerrainView::TerrainView(MainWindow * m, MapScene * s): BasicView(m, s)
{
}

void TerrainView::update()
{
	auto map = main->getMap();
	if(!map)
		return;

	pixmap.reset(new QPixmap(map->width * 32, map->height * 32));
	draw(false);
}

void TerrainView::setDirty(const int3 & tile)
{
	dirty.insert(tile);
}

void TerrainView::draw(bool onlyDirty)
{
	if(!pixmap)
		return;

	auto map = main->getMap();
	if(!map)
		return;

	QPainter painter(pixmap.get());
	painter.setCompositionMode(QPainter::CompositionMode_Source);

	if(onlyDirty)
	{
		std::set<int3> forRedrawing(dirty), neighbours;
		for(auto & t : dirty)
		{
			for(auto & tt : int3::getDirs())
			{
				if(map->isInTheMap(t + tt))
					neighbours.insert(t + tt);
			}
		}
		for(auto & t : neighbours)
		{
			for(auto & tt : int3::getDirs())
			{
				forRedrawing.insert(t);
				if(map->isInTheMap(t + tt))
					forRedrawing.insert(t + tt);
			}
		}
		for(auto & t : forRedrawing)
		{
			main->getMapHandler()->drawTerrainTile(painter, t.x, t.y, scene->level);
		}
	}
	else
	{
		for(int j = 0; j < map->height; ++j)
			for(int i = 0; i < map->width; ++i)
				main->getMapHandler()->drawTerrainTile(painter, i, j, scene->level);
	}

	dirty.clear();
	redraw();
}

ObjectsView::ObjectsView(MainWindow * m, MapScene * s): BasicView(m, s)
{
}

void ObjectsView::update()
{
	auto map = main->getMap();
	if(!map)
		return;

	pixmap.reset(new QPixmap(map->width * 32, map->height * 32));
	pixmap->fill(QColor(0, 0, 0, 0));
	draw(false);
}

void ObjectsView::draw(bool onlyDirty)
{
	if(!pixmap)
		return;

	auto map = main->getMap();
	if(!map)
		return;

	pixmap->fill(QColor(0, 0, 0, 0));
	QPainter painter(pixmap.get());
	//painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	std::set<const CGObjectInstance *> drawen;


	for(int j = 0; j < map->height; ++j)
	{
		for(int i = 0; i < map->width; ++i)
		{
			main->getMapHandler()->drawObjects(painter, i, j, scene->level);
			/*auto & objects = main->getMapHandler()->getObjects(i, j, scene->level);
			for(auto & object : objects)
			{
				if(!object.obj || drawen.count(object.obj))
					continue;

				if(!onlyDirty || dirty.count(object.obj))
				{
					main->getMapHandler()->drawObject(painter, object);
					drawen.insert(object.obj);
				}
			}*/
		}
	}

	dirty.clear();
	redraw();
}

void ObjectsView::setDirty(int x, int y)
{
	auto & objects = main->getMapHandler()->getObjects(x, y, scene->level);
	for(auto & object : objects)
	{
		if(object.obj)
			dirty.insert(object.obj);
	}
}

void ObjectsView::setDirty(const CGObjectInstance * object)
{
	dirty.insert(object);
}

SelectionObjectsView::SelectionObjectsView(MainWindow * m, MapScene * s): BasicView(m, s)
{
}

void SelectionObjectsView::update()
{
	auto map = main->getMap();
	if(!map)
		return;

	selectedObjects.clear();

	pixmap.reset(new QPixmap(map->width * 32, map->height * 32));
	//pixmap->fill(QColor(0, 0, 0, 0));

	draw();
}

void SelectionObjectsView::draw()
{
	if(!pixmap)
		return;

	pixmap->fill(QColor(0, 0, 0, 0));

	QPainter painter(pixmap.get());
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.setPen(QColor(255, 255, 255));

	for(auto * obj : selectedObjects)
	{
		QRect bbox(obj->getPosition().x, obj->getPosition().y, 1, 1);
		for(auto & t : obj->getBlockedPos())
		{
			QPoint topLeft(std::min(t.x, bbox.topLeft().x()), std::min(t.y, bbox.topLeft().y()));
			bbox.setTopLeft(topLeft);
			QPoint bottomRight(std::max(t.x, bbox.bottomRight().x()), std::max(t.y, bbox.bottomRight().y()));
			bbox.setBottomRight(bottomRight);
		}

		painter.setOpacity(1.0);
		painter.drawRect(bbox.x() * 32, bbox.y() * 32, bbox.width() * 32, bbox.height() * 32);

		//show translation
		if(shift.x() || shift.y())
		{
			painter.setOpacity(0.5);
			auto newPos = QPoint(obj->getPosition().x, obj->getPosition().y) + shift;
			main->getMapHandler()->drawObjectAt(painter, obj, newPos.x(), newPos.y());
		}
	}

	redraw();
}

CGObjectInstance * SelectionObjectsView::selectObjectAt(int x, int y)
{
	if(!main->getMap() || !main->getMapHandler())
		return nullptr;

	auto & objects = main->getMapHandler()->getObjects(x, y, scene->level);

	//visitable is most important
	for(auto & object : objects)
	{
		if(!object.obj || selectedObjects.count(object.obj))
			continue;

		if(object.obj->visitableAt(x, y))
		{
			selectedObjects.insert(objects.back().obj);
			return object.obj;
		}
	}

	//if not visitable tile - try to get blocked
	for(auto & object : objects)
	{
		if(!object.obj || selectedObjects.count(object.obj))
			continue;

		if(object.obj->blockingAt(x, y))
		{
			selectedObjects.insert(objects.back().obj);
			return object.obj;
		}
	}

	//finally, we can take any object
	for(auto & object : objects)
	{
		if(!object.obj || selectedObjects.count(object.obj))
			continue;

		if(object.obj->coveringAt(x, y))
		{
			selectedObjects.insert(objects.back().obj);
			return object.obj;
		}
	}

	return nullptr;
}

std::set<CGObjectInstance *> SelectionObjectsView::selectObjects(int x1, int y1, int x2, int y2)
{
	std::set<CGObjectInstance *> result;
	//TBD
	return result;
}

void SelectionObjectsView::clear()
{
	selectedObjects.clear();
	shift.setX(0);
	shift.setY(0);
}
