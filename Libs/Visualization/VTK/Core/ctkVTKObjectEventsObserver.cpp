/*=========================================================================

  Library:   CTK

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

// Qt includes
#include <QStringList>
#include <QVariant>
#include <QList>
#include <QHash>
#include <QDebug>

// CTK includes
#include "ctkVTKObjectEventsObserver.h"
#include "ctkVTKConnection.h"

// VTK includes
#include <vtkObject.h>

//-----------------------------------------------------------------------------
CTK_SINGLETON_DEFINE(ctkVTKConnectionFactory)

//-----------------------------------------------------------------------------
// ctkVTKConnectionFactory

//-----------------------------------------------------------------------------
ctkVTKConnectionFactory* ctkVTKConnectionFactory::instance()
{
  return Self::Instance;
}

//-----------------------------------------------------------------------------
void ctkVTKConnectionFactory::setInstance(ctkVTKConnectionFactory* newInstance)
{
  if (!newInstance)
    {
    qCritical() << "ctkVTKConnectionFactory::setInstance - Failed to set a null instance !";
    return;
    }
  delete Self::Instance;
  Self::Instance = newInstance;
}

//-----------------------------------------------------------------------------
ctkVTKConnectionFactory::ctkVTKConnectionFactory()
{
}

//-----------------------------------------------------------------------------
ctkVTKConnectionFactory::~ctkVTKConnectionFactory()
{
}

//-----------------------------------------------------------------------------
ctkVTKConnection* ctkVTKConnectionFactory::createConnection(ctkVTKObjectEventsObserver* parent)const
{
  return new ctkVTKConnection(parent);
}

//-----------------------------------------------------------------------------
// ctkVTKObjectEventsObserverPrivate

//-----------------------------------------------------------------------------
class ctkVTKObjectEventsObserverPrivate
{
  Q_DECLARE_PUBLIC(ctkVTKObjectEventsObserver);
protected:
  ctkVTKObjectEventsObserver* const q_ptr;
public:
  typedef std::multimap<unsigned long, ctkVTKConnection*> ConnectionIndexType; // first: originating vtkObject, second: QT connection object name
  ctkVTKObjectEventsObserverPrivate(ctkVTKObjectEventsObserver& object);

  ///
  /// Return a reference toward the corresponding connection or 0 if doesn't exist
  ctkVTKConnection* findConnection(const QString& id)const;

  ///
  /// Return a reference toward the corresponding connection or 0 if doesn't exist
  ctkVTKConnection* findConnection(vtkObject* vtk_obj, unsigned long vtk_event,
    const QObject* qt_obj, const char* qt_slot)const;

  ///
  /// Return all the references that match the given parameters
  QList<ctkVTKConnection*> findConnections(vtkObject* vtk_obj, unsigned long vtk_event,
    const QObject* qt_obj, const char* qt_slot)const;

  inline QList<ctkVTKConnection*> connections()const
  {
    Q_Q(const ctkVTKObjectEventsObserver);
    return q->findChildren<ctkVTKConnection*>();
  }

  static unsigned long generateConnectionIndexHash(vtkObject* vtk_obj, unsigned long vtk_event,
    const QObject* qt_obj, const char* qt_slot)
  {
    return (unsigned char*)vtk_obj-(unsigned char*)qt_obj+vtk_event;
  }

  bool StrictTypeCheck;
  bool AllBlocked;
  bool ObserveDeletion;
  
  mutable ConnectionIndexType ConnectionIndex;
};

//-----------------------------------------------------------------------------
// ctkVTKObjectEventsObserverPrivate methods

//-----------------------------------------------------------------------------
ctkVTKObjectEventsObserverPrivate::ctkVTKObjectEventsObserverPrivate(ctkVTKObjectEventsObserver& object)
  :q_ptr(&object)
{
  this->StrictTypeCheck = false;
  this->AllBlocked = false;
  // ObserveDeletion == false  hasn't been that well tested...
  this->ObserveDeletion = true;
}

//-----------------------------------------------------------------------------
ctkVTKConnection*
ctkVTKObjectEventsObserverPrivate::findConnection(const QString& id)const
{
  foreach(ctkVTKConnection* connection, this->connections())
    {
    if (connection->id() == id)
      {
      return connection;
      }
    }
  return 0;
}

//-----------------------------------------------------------------------------
ctkVTKConnection*
ctkVTKObjectEventsObserverPrivate::findConnection(
  vtkObject* vtk_obj, unsigned long vtk_event,
  const QObject* qt_obj, const char* qt_slot)const
{
  // Linear search for connections is prohibitively slow when observing many objects
  Q_Q(const ctkVTKObjectEventsObserver);

  if(vtk_obj != NULL && qt_slot != NULL &&
     qt_obj != NULL && vtk_event != vtkCommand::NoEvent)
  {
    // All information is specified, so we can use the index to find the connection
    std::pair<ConnectionIndexType::iterator, ConnectionIndexType::iterator> rangeConnectionsForObject;
    rangeConnectionsForObject = this->ConnectionIndex.equal_range(generateConnectionIndexHash(vtk_obj, vtk_event, qt_obj, qt_slot));
    for (ConnectionIndexType::iterator connectionForObjectIt = rangeConnectionsForObject.first; 
      connectionForObjectIt != rangeConnectionsForObject.second;
      /*upon deletion the increment is done already, so don't increment here*/)
    {
      ctkVTKConnection* connection=connectionForObjectIt->second;
      if (!q->children().contains(connection))
      {
        // connection has been deleted, so remove it from the index
        connectionForObjectIt=this->ConnectionIndex.erase(connectionForObjectIt);
        continue;
      }
      if (connection->isEqual(vtk_obj, vtk_event, qt_obj, qt_slot))
      {        
        return connection;
      }
      ++connectionForObjectIt;
    }
    return 0;
  }

  foreach (ctkVTKConnection* connection, this->connections())
    {
    if (connection->isEqual(vtk_obj, vtk_event, qt_obj, qt_slot))
      {
      return connection;
      }
    }

  return 0;
}

//-----------------------------------------------------------------------------
QList<ctkVTKConnection*>
ctkVTKObjectEventsObserverPrivate::findConnections(
  vtkObject* vtk_obj, unsigned long vtk_event,
  const QObject* qt_obj, const char* qt_slot)const
{
  Q_Q(const ctkVTKObjectEventsObserver);

  bool all_info = true;
  if(vtk_obj == NULL || qt_slot == NULL ||
     qt_obj == NULL || vtk_event == vtkCommand::NoEvent)
    {
    all_info = false;
    }

  QList<ctkVTKConnection*> foundConnections;

  if (all_info)
  {
    ctkVTKConnection* connection=findConnection(vtk_obj, vtk_event, qt_obj, qt_slot);
    if (connection)
    {
      foundConnections.append(connection);
    }
    return foundConnections;
  }

  // Loop through all connection
  foreach (ctkVTKConnection* connection, this->connections())
    {
    if (connection->isEqual(vtk_obj, vtk_event, qt_obj, qt_slot))
      {
      foundConnections.append(connection);
      if (all_info)
        {
        break;
        }
      }
    }

  return foundConnections;
}

//-----------------------------------------------------------------------------
// ctkVTKObjectEventsObserver methods

//-----------------------------------------------------------------------------
ctkVTKObjectEventsObserver::ctkVTKObjectEventsObserver(QObject* _parent):Superclass(_parent)
  , d_ptr(new ctkVTKObjectEventsObserverPrivate(*this))
{
  this->setProperty("QVTK_OBJECT", true);
}

//-----------------------------------------------------------------------------
ctkVTKObjectEventsObserver::~ctkVTKObjectEventsObserver()
{
}

//-----------------------------------------------------------------------------
void ctkVTKObjectEventsObserver::printAdditionalInfo()
{
  this->Superclass::dumpObjectInfo();
  Q_D(ctkVTKObjectEventsObserver);
  qDebug() << "ctkVTKObjectEventsObserver:" << this << endl
           << " AllBlocked:" << d->AllBlocked << endl
           << " Parent:" << (this->parent()?this->parent()->objectName():"NULL") << endl
           << " Connection count:" << d->connections().count();

  // Loop through all connection
  foreach (const ctkVTKConnection* connection, d->connections())
    {
    qDebug() << *connection;
    }
}

//-----------------------------------------------------------------------------
bool ctkVTKObjectEventsObserver::strictTypeCheck()const
{
  Q_D(const ctkVTKObjectEventsObserver);
  return d->StrictTypeCheck;
}

//-----------------------------------------------------------------------------
void ctkVTKObjectEventsObserver::setStrictTypeCheck(bool check)
{
  Q_D(ctkVTKObjectEventsObserver);
  d->StrictTypeCheck = check;
}

//-----------------------------------------------------------------------------
QString ctkVTKObjectEventsObserver::addConnection(vtkObject* old_vtk_obj, vtkObject* vtk_obj,
  unsigned long vtk_event, const QObject* qt_obj, const char* qt_slot, float priority,
  Qt::ConnectionType connectionType)
{
  Q_D(ctkVTKObjectEventsObserver);
  if (old_vtk_obj)
    {
    // Check that old_object and new_object are the same type
    // If you have a crash when accessing old_vtk_obj->GetClassName(), that means
    // old_vtk_obj has been deleted and you should probably have keep
    // old_vtk_obj into a vtkWeakPointer:
    // vtkWeakPointer<vtkObject> obj1 = myobj1;
    // this->addConnection(obj1, vtkCommand::Modified...)
    // myobj1->Delete();
    // vtkWeakPointer<vtkObject> obj2 = myobj2;
    // this->addConnection(obj1, obj2, vtkCommand::Modified...)
    // ...
    // Or just call addConnection with a new
    // vtk_obj of 0 before the vtk_obj is deleted.
    // vtkObject* obj1 = vtkObject::New();
    // this->addConnection(obj1, vtkCommand::Modified...)
    // this->addConnection(obj1, 0, vtkCommand::Modified...)
    // obj1->Delete();
    // vtkObject* obj2 = vtkObject::New();
    // this->addConnection(0, obj2, vtkCommand::Modified...)
    // ...
    if (d->StrictTypeCheck && vtk_obj
        && !vtk_obj->IsA(old_vtk_obj->GetClassName()))
      {
      qWarning() << "Previous vtkObject (type:" << old_vtk_obj->GetClassName()
                 << ") to disconnect"
                 << "and new vtkObject (type:" << vtk_obj->GetClassName()
                 << ") to connect"
                 << "have a different type.";
      return QString();
      }
    // Disconnect old vtkObject
    this->removeConnection(old_vtk_obj, vtk_event, qt_obj, qt_slot);
    }
  return this->addConnection(
    vtk_obj, vtk_event, qt_obj, qt_slot, priority, connectionType);
}

//-----------------------------------------------------------------------------
QString ctkVTKObjectEventsObserver::reconnection(vtkObject* vtk_obj,
  unsigned long vtk_event, const QObject* qt_obj,
  const char* qt_slot, float priority, Qt::ConnectionType connectionType)
{
  this->removeConnection(0, vtk_event, qt_obj, qt_slot);
  return this->addConnection(
    vtk_obj, vtk_event, qt_obj, qt_slot, priority, connectionType);
}

//-----------------------------------------------------------------------------
QString ctkVTKObjectEventsObserver::addConnection(vtkObject* vtk_obj, unsigned long vtk_event,
  const QObject* qt_obj, const char* qt_slot, float priority, Qt::ConnectionType connectionType)
{
  Q_D(ctkVTKObjectEventsObserver);
  // If no vtk_obj is provided, there is no way we can create a connection.
  if (!vtk_obj)
    {
    return QString();
    }
  if (!ctkVTKConnection::isValid(vtk_obj, vtk_event, qt_obj, qt_slot))
    {
    qDebug() << "ctkVTKObjectEventsObserver::addConnection(...) - Invalid parameters - "
             << ctkVTKConnection::shortDescription(vtk_obj, vtk_event, qt_obj, qt_slot);
    return QString();
    }

  // Check if such event is already observed
  if (this->containsConnection(vtk_obj, vtk_event, qt_obj, qt_slot))
    {
    // if you need to have more than 1 connection, then it's probably time to
    // add the same mechanism than Qt does: Qt::UniqueConnection
    //qWarning() << "ctkVTKObjectEventsObserver::addConnection - [vtkObject:"
    //           << vtk_obj->GetClassName()
    //           << ", event:" << vtk_event << "]"
    //           << " is already connected with [qObject:" << qt_obj->objectName()
    //           << ", slot:" << qt_slot << "]";
    return QString();
    }

  // Instantiate a new connection, set its parameters and add it to the list
  ctkVTKConnection * connection = ctkVTKConnectionFactory::instance()->createConnection(this);
  QString objName=connection->objectName();
  d->ConnectionIndex.insert(ctkVTKObjectEventsObserverPrivate::ConnectionIndexType::value_type(
    ctkVTKObjectEventsObserverPrivate::generateConnectionIndexHash(vtk_obj, vtk_event, qt_obj, qt_slot), connection));
  
  connection->observeDeletion(d->ObserveDeletion);
  connection->setup(vtk_obj, vtk_event, qt_obj, qt_slot, priority, connectionType);

  // If required, establish connection
  connection->setBlocked(d->AllBlocked);

  return connection->id();
}

//-----------------------------------------------------------------------------
bool ctkVTKObjectEventsObserver::blockAllConnections(bool block)
{
  Q_D(ctkVTKObjectEventsObserver);

  if (d->AllBlocked == block)
    {
    return d->AllBlocked;
    }

  bool oldAllBlocked = d->AllBlocked;

  foreach (ctkVTKConnection* connection, d->connections())
    {
    connection->setBlocked(block);
    }
  d->AllBlocked = block;
  return oldAllBlocked;
}

//-----------------------------------------------------------------------------
bool ctkVTKObjectEventsObserver::connectionsBlocked()const
{
  Q_D(const ctkVTKObjectEventsObserver);
  return d->AllBlocked;
}

//-----------------------------------------------------------------------------
bool ctkVTKObjectEventsObserver::blockConnection(const QString& id, bool blocked)
{
  Q_D(ctkVTKObjectEventsObserver);
  ctkVTKConnection* connection = d->findConnection(id);
  if (connection == 0)
    {
    qWarning() << "no connection for id " << id;
    return false;
    }
  bool oldBlocked = connection->isBlocked();
  connection->setBlocked(blocked);
  return oldBlocked;
}

//-----------------------------------------------------------------------------
int ctkVTKObjectEventsObserver::blockConnection(bool block, vtkObject* vtk_obj,
  unsigned long vtk_event, const QObject* qt_obj)
{
  Q_D(ctkVTKObjectEventsObserver);
  if (!vtk_obj)
    {
    qDebug() << "ctkVTKObjectEventsObserver::blockConnectionRecursive"
             << "- Failed to " << (block?"block":"unblock") <<" connection"
             << "- vtkObject is NULL";
    return 0;
    }
  QList<ctkVTKConnection*> connections =
    d->findConnections(vtk_obj, vtk_event, qt_obj, 0);
  foreach (ctkVTKConnection* connection, connections)
    {
    connection->setBlocked(block);
    }
  return connections.size();
}

//-----------------------------------------------------------------------------
int ctkVTKObjectEventsObserver::removeConnection(vtkObject* vtk_obj, unsigned long vtk_event,
    const QObject* qt_obj, const char* qt_slot)
{
  Q_D(ctkVTKObjectEventsObserver);

  QList<ctkVTKConnection*> connections =
    d->findConnections(vtk_obj, vtk_event, qt_obj, qt_slot);

  foreach (ctkVTKConnection* connection, connections)
    {    
    // no need to update the index, it'll be updated on-the fly when searching for connections
    delete connection;
    }
  return connections.count();
}

//-----------------------------------------------------------------------------
int ctkVTKObjectEventsObserver::removeAllConnections()
{
  return this->removeConnection(0, vtkCommand::NoEvent, 0, 0);
}

//-----------------------------------------------------------------------------
bool ctkVTKObjectEventsObserver::containsConnection(vtkObject* vtk_obj, unsigned long vtk_event,
  const QObject* qt_obj, const char* qt_slot)const
{
  Q_D(const ctkVTKObjectEventsObserver);
  return (d->findConnection(vtk_obj, vtk_event, qt_obj, qt_slot) != 0);
}
