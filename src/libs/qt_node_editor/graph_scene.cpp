#include "graph_scene.h"

#include <cassert>
#include <iostream>

#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

#include "node.h"
#include "connected_edge.h"

namespace node_editor {

GraphScene::GraphScene(QGraphicsView* parent) : QGraphicsScene(parent), m_leftMouseDown(false) {
	m_editedEdge = new Edge(QPointF(0, 0), QPointF(0, 0));
	m_editedEdge->setVisible(false);
	addItem(m_editedEdge);

	m_infoRect = new QGraphicsRectItem();
	m_infoRect->setBrush(QColor(24, 24, 24));
	m_infoRect->setZValue(1000);
	m_infoRect->setVisible(false);

	m_infoText = new QGraphicsTextItem(m_infoRect);

	addItem(m_infoRect);

	QObject::connect(this, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
}

GraphScene::~GraphScene() {
	clear();

	assert(m_nodes.empty());
	assert(m_edges.empty());
}

Node& GraphScene::node(unsigned index) {
	assert(index < (unsigned)m_nodes.size());
	return *m_nodes[index];
}

const Node& GraphScene::node(unsigned index) const {
	assert(index < (unsigned)m_nodes.size());
	return *m_nodes[index];
}

unsigned GraphScene::nodeCount() const {
	return m_nodes.size();
}

ConnectedEdge& GraphScene::edge(unsigned index) {
	assert(index < (unsigned)m_edges.size());
	return *m_edges[index];
}

const ConnectedEdge& GraphScene::edge(unsigned index) const {
	assert(index < (unsigned)m_edges.size());
	return *m_edges[index];
}

unsigned GraphScene::edgeCount() const {
	return m_edges.size();
}

Node& GraphScene::addNode(const QString& name,
                          const QPointF& position,
                          const std::initializer_list<Node::PortDefinition>& ports) {

	Node* n = new Node(name, position, ports);
	m_nodes.push_back(n);
	addItem(n);

	return *n;
}

void GraphScene::removeNode(Node& n) {
	auto i = m_nodes.begin();
	while(i != m_nodes.end())
		if(*i == &n)
			delete *i;
		else
			++i;

	assert(std::find(m_nodes.begin(), m_nodes.end(), &n) == m_nodes.end());
}

void GraphScene::connect(Port& p1, Port& p2) {
	if((p1.portType() & Port::kOutput) && (p2.portType() & Port::kInput)) {
		if(!isConnected(p1, p2)) {
			ConnectedEdge* e = new ConnectedEdge(p1, p2);
			m_edges.push_back(e);
			addItem(e);
		}
	}
}

void GraphScene::disconnect(Port& p1, Port& p2) {
	int index = 0;
	while(index < m_edges.size()) {
		auto* edge = m_edges[index];
		if((&edge->fromPort() == &p1) && (&edge->toPort() == &p2))
			delete edge;
		else
			++index;
	}
}

void GraphScene::disconnect(ConnectedEdge& e) {
	auto it = std::find(m_edges.begin(), m_edges.end(), &e);
	if(it != m_edges.end())
		delete *it;

	assert(std::find(m_edges.begin(), m_edges.end(), &e) == m_edges.end());
}

bool GraphScene::isConnected(const Port& p1, const Port& p2) {
	auto it = std::find_if(m_edges.begin(), m_edges.end(), [&](const ConnectedEdge * e) {
		return &e->fromPort() == &p1 && &e->toPort() == &p2;
	});

	return it != m_edges.end();
}

void GraphScene::remove(Node* n) {
	auto it = std::find(m_nodes.begin(), m_nodes.end(), n);
	assert(it != m_nodes.end());
	m_nodes.erase(it);
}

void GraphScene::remove(Edge* e) {
	auto it = std::find(m_edges.begin(), m_edges.end(), e);
	if(it != m_edges.end())
		m_edges.erase(it);
}

namespace {

template<typename ITEM>
ITEM* findItem(QGraphicsItem* item) {
	ITEM* result = NULL;
	while(item && !result) {
		result = dynamic_cast<ITEM*>(item);
		item = item->parentItem();
	}
	return result;
}

}

Port* GraphScene::findConnectionPort(QPointF pos) const {
	auto it = items(pos);
	for(auto& i : it) {
		Port* port = findItem<Port>(i);
		if(port)
			return port;
	}

	return NULL;
}

QPointF GraphScene::findConnectionPoint(QPointF pos, Port::Type portType) const {
	// try to find a port under the mouse
	Port* port = findConnectionPort(pos);

	// if a port was found, and it was the opposite type than portType, snap
	if(port && (!(port->portType() & portType) || port->portType() == Port::kInputOutput)) {
		const QRectF bbox = port->boundingRect();
		if(portType == Port::kOutput)
			pos = QPointF(bbox.x() + bbox.height() / 2, bbox.y() + bbox.height() / 2);
		else
			pos = QPointF(bbox.x() + bbox.width() - bbox.height() / 2, bbox.y() + bbox.height() / 2);
		pos = port->mapToScene(pos);
	}

	return pos;
}

void GraphScene::mousePressEvent(QGraphicsSceneMouseEvent* mouseEvent) {
	if(mouseEvent->button() == Qt::LeftButton) {
		Port* port = findItem<Port>(itemAt(mouseEvent->scenePos(), QTransform()));
		if(port) {
			Port::Type portType;
			if((port->portType() == Port::kInput) || (port->portType() == Port::kOutput))
				portType = port->portType();
			else {
				const QPointF pos = port->mapFromScene(mouseEvent->scenePos());
				const QRectF bbox = port->boundingRect();
				if(pos.x() < bbox.width() / 2)
					portType = Port::kInput;
				else
					portType = Port::kOutput;
			}

			QPointF pos;
			{
				const QRectF bbox = port->boundingRect();
				if(portType == Port::kInput)
					pos = QPointF(bbox.x() + bbox.height() / 2, bbox.y() + bbox.height() / 2);
				else
					pos = QPointF(bbox.x() + bbox.width() - bbox.height() / 2, bbox.y() + bbox.height() / 2);
				pos = port->mapToScene(pos);
			}

			m_editedEdge->setVisible(true);
			m_editedEdge->setPoints(pos, pos);
			m_editedEdge->setPen(QPen(port->color(), 2));
			m_connectedSide = portType;

			mouseEvent->accept();
		}
		else {
			m_editedEdge->setVisible(false);

			m_leftMouseDown = true;

			QGraphicsScene::mousePressEvent(mouseEvent);
		}
	}
	else if(mouseEvent->button() == Qt::MiddleButton) {
		Node* node = findItem<Node>(itemAt(mouseEvent->scenePos(), QTransform()));
		if(node && m_nodeInfoCallback) {
			m_infoText->setHtml(m_nodeInfoCallback(*node).c_str());
			m_infoRect->setPos(mouseEvent->scenePos().x() - m_infoText->boundingRect().width()/2, mouseEvent->scenePos().y());
			m_infoRect->setRect(m_infoText->boundingRect());
			m_infoRect->setVisible(true);

			QApplication::setOverrideCursor(Qt::BlankCursor);
		}

		mouseEvent->accept();
	}
	else
		QGraphicsScene::mousePressEvent(mouseEvent);
}

void GraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent* mouseEvent) {
	if(m_editedEdge->isVisible()) {
		if(m_connectedSide == Port::kInput)
			m_editedEdge->setPoints(findConnectionPoint(mouseEvent->scenePos(), m_connectedSide), m_editedEdge->target());
		else
			m_editedEdge->setPoints(m_editedEdge->origin(), findConnectionPoint(mouseEvent->scenePos(), m_connectedSide));

		mouseEvent->accept();
	}
	else
		QGraphicsScene::mouseMoveEvent(mouseEvent);
}

void GraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* mouseEvent) {
	if(m_infoRect->isVisible()) {
		m_infoRect->setVisible(false);

		QApplication::restoreOverrideCursor();
	}

	if(m_editedEdge->isVisible()) {
		m_editedEdge->setVisible(false);

		Port* portFrom = findConnectionPort(m_editedEdge->origin());
		Port* portTo = findConnectionPort(m_editedEdge->target());

		if(portFrom != NULL && portTo != NULL && portFrom != portTo &&
		        portFrom->portType() & Port::kOutput && portTo->portType() & Port::kInput) {

			if(!isConnected(*portFrom, *portTo)) {
				connect(*portFrom, *portTo);

				if(m_connectionCallback)
					m_connectionCallback(*portFrom, *portTo);
			}
		}

		mouseEvent->accept();
	}
	else {
		if(mouseEvent->button() == Qt::LeftButton) {
			if(m_leftMouseDown) {
				m_leftMouseDown = false;

				if(!m_movingNodes.empty() && m_nodesMoveCallback)
					m_nodesMoveCallback(m_movingNodes);
				m_movingNodes.clear();
			}
		}

		QGraphicsScene::mouseReleaseEvent(mouseEvent);
	}
}

bool GraphScene::isEdgeEditInProgress() const {
	return m_editedEdge->isVisible();
}

void GraphScene::setMouseConnectionCallback(std::function<void(Port&, Port&)> fn) {
	m_connectionCallback = fn;
}

void GraphScene::setNodesMoveCallback(std::function<void(const std::set<Node*>&)> fn) {
	m_nodesMoveCallback = fn;
}

void GraphScene::onSelectionChanged() {
	Selection selectionSet;

	QList<QGraphicsItem*> selection = selectedItems();
	for(auto& i : selection) {
		Node* node = dynamic_cast<Node*>(i);
		if(node)
			selectionSet.nodes.insert(node);

		ConnectedEdge* edge = dynamic_cast<ConnectedEdge*>(i);
		if(edge)
			selectionSet.connections.insert(edge);
	}

	selectionChanged(selectionSet);
}

void GraphScene::setNodeInfoCallback(std::function<std::string(const Node&)> fn) {
	m_nodeInfoCallback = fn;
}

void GraphScene::registerNodeMove(Node* n) {
	if(m_leftMouseDown)
		m_movingNodes.insert(n);
}

}
