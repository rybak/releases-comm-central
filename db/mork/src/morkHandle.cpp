/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKFACTORY_
#include "morkFactory.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` `````
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkHandle::CloseMorkNode(morkEnv* ev) // CloseHandle() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseHandle(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkHandle::~morkHandle() // assert CloseHandle() executed earlier
{
  MORK_ASSERT(mHandle_Env==0);
  MORK_ASSERT(mHandle_Face==0);
  MORK_ASSERT(mHandle_Object==0);
  MORK_ASSERT(mHandle_Magic==0);
  MORK_ASSERT(mHandle_Tag==morkHandle_kTag); // should still have correct tag
}

/*public non-poly*/
morkHandle::morkHandle(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,  // must not be nil, cookie for this handle
    morkObject* ioObject,    // must not be nil, the object for this handle
    mork_magic inMagic)      // magic sig to denote specific subclass
: morkNode(ev, morkUsage::kPool, (nsIMdbHeap*) 0L)
, mHandle_Tag( 0 )
, mHandle_Env( ev )
, mHandle_Face( ioFace )
, mHandle_Object( 0 )
, mHandle_Magic( 0 )
{
  if ( ioFace && ioObject )
  {
    if ( ev->Good() )
    {
      mHandle_Tag = morkHandle_kTag;
      morkObject::SlotStrongObject(ioObject, ev, &mHandle_Object);
      morkHandle::SlotWeakHandle(this, ev, &ioObject->mObject_Handle);
      if ( ev->Good() )
      {
        mHandle_Magic = inMagic;
        mNode_Derived = morkDerived_kHandle;
      }
    }
    else
      ev->CantMakeWhenBadError();
  }
  else
    ev->NilPointerError();
}

/*public non-poly*/ void
morkHandle::CloseHandle(morkEnv* ev) // called by CloseMorkNode();
{
    if ( this->IsNode() )
    {
      morkObject* obj = mHandle_Object;
      mork_bool objDidRefSelf = ( obj && obj->mObject_Handle == this );
      if ( objDidRefSelf )
        obj->mObject_Handle = 0; // drop the reference

      morkObject::SlotStrongObject((morkObject*) 0, ev, &mHandle_Object);
      mHandle_Magic = 0;
      // note mHandle_Tag MUST stay morkHandle_kTag for morkNode::ZapOld()
      this->MarkShut();

      if ( objDidRefSelf )
        this->CutWeakRef(ev); // do last, because it might self destroy
    }
    else
      this->NonNodeError(ev);
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` `````

void morkHandle::NilFactoryError(morkEnv* ev) const
{
  ev->NewError("nil mHandle_Factory");
}

void morkHandle::NilHandleObjectError(morkEnv* ev) const
{
  ev->NewError("nil mHandle_Object");
}

void morkHandle::NonNodeObjectError(morkEnv* ev) const
{
  ev->NewError("non-node mHandle_Object");
}

void morkHandle::NonOpenObjectError(morkEnv* ev) const
{
  ev->NewError("non-open mHandle_Object");
}

void morkHandle::NewBadMagicHandleError(morkEnv* ev, mork_magic inMagic) const
{
  MORK_USED_1(inMagic);
  ev->NewError("wrong mHandle_Magic");
}

void morkHandle::NewDownHandleError(morkEnv* ev) const
{
  if ( this->IsHandle() )
  {
    if ( this->GoodHandleTag() )
    {
      if ( this->IsOpenNode() )
        ev->NewError("unknown down morkHandle error");
      else
        this->NonOpenNodeError(ev);
    }
    else
      ev->NewError("wrong morkHandle tag");
  }
  else
    ev->NewError("non morkHandle");
}

morkObject* morkHandle::GetGoodHandleObject(morkEnv* ev,
  mork_bool inMutable, mork_magic inMagicType, mork_bool inClosedOkay) const
{
  morkObject* outObject = 0;
  if ( this->IsHandle() && this->GoodHandleTag() &&
    ( inClosedOkay || this->IsOpenNode() ) )
  {
    if ( !inMagicType || mHandle_Magic == inMagicType )
    {
      morkObject* obj = this->mHandle_Object;
      if ( obj )
      {
        if ( obj->IsNode() )
        {
          if ( inClosedOkay || obj->IsOpenNode() )
          {
            if ( this->IsMutable() || !inMutable )
              outObject = obj;
            else
              this->NonMutableNodeError(ev);
          }
          else
            this->NonOpenObjectError(ev);
        }
        else
          this->NonNodeObjectError(ev);
      }
      else if ( !inClosedOkay )
        this->NilHandleObjectError(ev);
    }
    else
      this->NewBadMagicHandleError(ev, inMagicType);
  }
  else
    this->NewDownHandleError(ev);

  MORK_ASSERT(outObject || inClosedOkay);
  return outObject;
}


morkEnv*
morkHandle::CanUseHandle(nsIMdbEnv* mev, mork_bool inMutable,
                         mork_bool inClosedOkay, nsresult* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkObject* obj = this->GetGoodHandleObject(ev, inMutable,
      /*magic*/ 0, inClosedOkay);
    if ( obj )
    {
      outEnv = ev;
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv || inClosedOkay);
  return outEnv;
}

// { ===== begin nsIMdbObject methods =====

// { ----- begin attribute methods -----
/*virtual*/ nsresult
morkHandle::Handle_IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  nsresult outErr = NS_OK;
  mdb_bool readOnly = mdbBool_kTrue;

  morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    readOnly = mHandle_Object->IsFrozen();

    outErr = ev->AsErr();
  }
  MORK_ASSERT(outIsReadonly);
  if ( outIsReadonly )
    *outIsReadonly = readOnly;

  return outErr;
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ nsresult
morkHandle::Handle_GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  nsresult outErr = NS_OK;
  nsIMdbFactory* handle = 0;

  morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    morkFactory* factory = ev->mEnv_Factory;
    if ( factory )
    {
      handle = factory;
      NS_ADDREF(handle);
    }
    else
      this->NilFactoryError(ev);

    outErr = ev->AsErr();
  }

  MORK_ASSERT(acqFactory);
  if ( acqFactory )
    *acqFactory = handle;

  return outErr;
}
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ nsresult
morkHandle::Handle_GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  nsresult outErr = NS_OK;
  mdb_count count = 0;

  morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    count = this->WeakRefsOnly();

    outErr = ev->AsErr();
  }
  MORK_ASSERT(outCount);
  if ( outCount )
    *outCount = count;

  return outErr;
}
/*virtual*/ nsresult
morkHandle::Handle_GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  nsresult outErr = NS_OK;
  mdb_count count = 0;

  morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    count = this->StrongRefsOnly();

    outErr = ev->AsErr();
  }
  MORK_ASSERT(outCount);
  if ( outCount )
    *outCount = count;

  return outErr;
}

/*virtual*/ nsresult
morkHandle::Handle_AddWeakRef(nsIMdbEnv* mev)
{
  nsresult outErr = NS_OK;

  morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    this->AddWeakRef(ev);
    outErr = ev->AsErr();
  }

  return outErr;
}
/*virtual*/ nsresult
morkHandle::Handle_AddStrongRef(nsIMdbEnv* mev)
{
  nsresult outErr = NS_OK;

  morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    this->AddStrongRef(ev);
    outErr = ev->AsErr();
  }

  return outErr;
}

/*virtual*/ nsresult
morkHandle::Handle_CutWeakRef(nsIMdbEnv* mev)
{
  nsresult outErr = NS_OK;

  morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    this->CutWeakRef(ev);
    outErr = ev->AsErr();
  }

  return outErr;
}
/*virtual*/ nsresult
morkHandle::Handle_CutStrongRef(nsIMdbEnv* mev)
{
  nsresult outErr = NS_OK;
  morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    this->CutStrongRef(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ nsresult
morkHandle::Handle_CloseMdbObject(nsIMdbEnv* mev)
// called at strong refs zero
{
  // if only one ref, Handle_CutStrongRef will clean up better.
  if (mNode_Uses == 1)
    return Handle_CutStrongRef(mev);

  nsresult outErr = NS_OK;

  if ( this->IsNode() && this->IsOpenNode() )
  {
    morkEnv* ev = CanUseHandle(mev, /*inMutable*/ morkBool_kFalse,
    /*inClosedOkay*/ morkBool_kTrue, &outErr);
    if ( ev )
    {
      morkObject* object = mHandle_Object;
      if ( object && object->IsNode() && object->IsOpenNode() )
        object->CloseMorkNode(ev);

      this->CloseMorkNode(ev);
      outErr = ev->AsErr();
    }
  }
  return outErr;
}

/*virtual*/ nsresult
morkHandle::Handle_IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  MORK_USED_1(mev);
  nsresult outErr = NS_OK;

  MORK_ASSERT(outOpen);
  if ( outOpen )
    *outOpen = this->IsOpenNode();

  return outErr;
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
