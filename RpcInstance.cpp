﻿#include "StdAfx.h"
#include "RpcInstance.h"
#include "RpcClient.h"
#include "Resource.h"
#include "RpcMains.h"
#include "Resource.h"
#include "RpcObject.h"
#include "RpcUtilities.h"
#include "RpcDocument.h"


CRpcInstance::CRpcInstance(const CRhinoDoc& doc, const CLBPString& sFullPath)
{
	Construct(doc.RuntimeSerialNumber(), NULL, sFullPath);
}

CRpcInstance::CRpcInstance(const CRhinoDoc& doc, const CRhinoObject& obj)
{
	Construct(doc.RuntimeSerialNumber(), &obj, L"");
}

CRpcInstance::~CRpcInstance()
{
	KillEditUi();
	delete m_pInstance;
}

void CRpcInstance::Construct(UINT idDoc, const CRhinoObject* pObject, const CLBPString& sRpcPath)
{
	m_pEditInterface = NULL;
	m_pEditDlgCallback = NULL;
	m_idObject = ON_nil_uuid;
	m_idDoc = idDoc;

	if (!sRpcPath.IsEmpty())
	{
		Mains().RpcClient().OnRpcInstanceWillBeCreated(m_idDoc, sRpcPath);
	}

	m_pInstance = dynamic_cast<RPCapi::Instance *>(Mains().RpcClient().RPCgetAPI()->newObject(RPCapi::ObjectCodes::INSTANCE));
	if (NULL == m_pInstance) return;

	m_pInstance->setClientInstance(this);

	if (!sRpcPath.IsEmpty())
	{
		if (!m_pInstance->setRPCFileName(sRpcPath.T()))
		{
			RPCapi::ParamList* pRpcFile = Mains().RpcClient().RPCgetAPI()->openRPCFile(sRpcPath.T());
			if (NULL != pRpcFile)
			{
				RPCapi::ID* pRpcId = dynamic_cast<RPCapi::ID*>(pRpcFile->get("/metadata/id"));
				delete pRpcFile;
				m_pInstance->setRpcId(pRpcId);
				delete pRpcId;
			}
		}
	}
	else
	if (NULL != pObject)
	{
		m_idObject	= pObject->ModelObjectId();
		CopyToRpc(*pObject);
	}
	else
	{
		return;
	}
}

bool CRpcInstance::IsValidRpc(const CLBPString& s) // static
{
	RPCapi* pApi = Mains().RpcClient().RPCgetAPI();
	if (NULL == pApi) return false;

	RPCapi::ParamList* pRpcFile = pApi->openRPCFile(s.T());
	if (NULL == pRpcFile) return false;

	delete pRpcFile;
	return true;
}

bool CRpcInstance::IsValid(void) const
{
	RPCapi* pApi = Mains().RpcClient().RPCgetAPI();
	if (NULL == pApi) return false;

	CLBPString sPath = FileName();
	if (sPath.IsEmpty())
	{
		pApi->updatePaths();
		sPath = FileName();
	}

	RPCapi::ParamList* pRpcFile = pApi->openRPCFile(sPath.T());
	if (NULL == pRpcFile)
	{
		return false;
	}

	delete pRpcFile;
	return true;
}

void CRpcInstance::SetId(const RPCapi::ID *id)
{
	if (NULL == m_pInstance)
		return;

	m_pInstance->setRpcId(id);
}

const RPCapi::ID* CRpcInstance::Id(void) const
{
	if (NULL == m_pInstance)
		return NULL;

	return m_pInstance->getID();
}

CLBPString CRpcInstance::FileName(void) const
{
	if (NULL == m_pInstance)
		return L"";

	const TString & s = m_pInstance->getRPCFileName();

	return s.getA();
}

CLBPString CRpcInstance::ObjectName(void) const
{
	const CRhinoObject* pObject = Object();

	if (NULL == pObject)
	{
		m_sName = L"";
	}
	else
	{
		m_sName = pObject->Attributes().m_name;
	}

	return m_sName;
}

CLBPString CRpcInstance::RpcName(void) const
{
	if (NULL == m_pInstance)
		return L"";

	TString* pName = m_pInstance->getName();
	if (NULL == pName) return L"";

	CLBPString s = pName->getA();
	delete pName;

	return s;
}

RPCapi::Instance* CRpcInstance::Instance(void)
{
	return m_pInstance;
}

TStringArg CRpcInstance::RPCgetName(void)
{
	ObjectName();
	return m_sName.AsMBCSString();
}

RPCapi::Instance* CRpcInstance::RPCgetInstance(void)
{
	return m_pInstance;
}

void CRpcInstance::RPCgetPivot(double px, double py, double pz, double &distance, double &dx, double &dy, double &dz)
{

}

int CRpcInstance::RPCgetTime(void)
{
	return Mains().RpcDocument().AnimationFrame();
}

bool CRpcInstance::RPCisSelected(void)
{
	const CRhinoObject* pObject = Object();
	if (NULL == pObject)
	{
		return false;
	}

	return (0 == pObject->IsSelected()) ? false : true;
}

void CRpcInstance::RPCparameterChangeNotification(bool newInstance, const TString **params, int num)
{
	if (NULL != m_pEditDlgCallback)
	{
		m_pEditDlgCallback->OnRpcParameterChanged();
	}
}

void CRpcInstance::RPCselect(bool select)
{
	const CRhinoObject* pObject = Object();
	if (NULL != pObject)
	{
		pObject->Select(select);
	}
}

bool CRpcInstance::Data(CLBPBuffer& buf) const
{
	if (!IsValid())
		return false;

	const int iSize = m_pInstance->size();
	char* buffer = new char[iSize + 1];
	buffer[iSize] = 0;

	stringstream streamOut(buffer);
	m_pInstance->toStream(streamOut, 0);

	string str = streamOut.str();
	const size_t strSize = str.size();

	buf.Set(str.c_str(), strSize);

	delete []buffer;

	return true;
}

void CRpcInstance::SetData(const CLBPBuffer& buf)
{
	if (NULL == m_pInstance)
		return;

	const int iType = m_pInstance->typeCode();
	const size_t iSize = buf.NumBytes();
	const BYTE* pBytes = buf.Bytes();

	const char* sz = (const char*)pBytes;

	string str(sz, iSize);
	stringstream streamIn(str);

	m_pInstance->fromStream(streamIn, 0, iType);

	Mains().RpcClient().OnRpcInstanceWillBeCreated(m_idDoc, FileName());
}

bool CRpcInstance::CopyToRpc(const CRhinoObject& obj)
{
	CRpcObject ro(&obj);
	if (!ro.IsTagged())
		return false;

	CLBPBuffer buf;
	ro.RpcData(buf);

	SetData(buf);

	return true;
}

bool CRpcInstance::CopyFromRpc(const CRhinoObject& obj) const
{
	const CLBPString sRpc = FileName();
	if (sRpc.IsEmpty())
		return false;

	CLBPBuffer buf;
	Data(buf);

	CRpcObject ro(&obj);
	ro.Tag(buf);

	return true;
}

void CRpcInstance::KillEditUi(void)
{
	if (NULL != m_pEditInterface)
	{
		m_pEditInterface->hide();
		delete m_pEditInterface;
		m_pEditInterface = NULL;
		m_pEditDlgCallback = NULL;
	}
}

bool CRpcInstance::EditUi(HWND hWndParent, IEditDialogCallback* pCallback)
{
	if (NULL == m_pInstance)
		return false;

	m_pEditDlgCallback = pCallback;

	KillEditUi();

	m_pEditInterface = dynamic_cast<RPCapiEx::InstanceInterfaceEx *>(Mains().RpcClient().RPCgetAPI()->newObject(RPCapi::ObjectCodes::INSTANCE_INTERFACE));
	if (NULL == m_pEditInterface) return false;

	m_pEditInterface->setInstance(m_pInstance);

	CRhinoDoc* pDoc = CRhinoDoc::FromRuntimeSerialNumber(m_idDoc);
	const double dScale = (NULL != pDoc) ? ON::UnitScale(ON::LengthUnitSystem::Inches, pDoc->ModelUnits()) : 1.0;

	m_pEditInterface->setUnits(RPCapi::Units::LINEAR_UNITS, dScale);
	m_pEditInterface->show(hWndParent, RPCapi::InstanceInterface::Window::PARAMETERS, RPCapiEx::InstanceInterfaceEx::WindowEx::MODE_CODE::MODELESS);

	return true;
}

CRhinoInstanceObject* CRpcInstance::Replace(CRhinoDoc& doc)
{
	const CRhinoInstanceObject* pBlock = CRhinoInstanceObject::Cast(Object());
	if (NULL == pBlock) return NULL;

	const int iInstanceDefintionId = pBlock->InstanceDefinition()->Index();
	const ON_Xform xformInstance = pBlock->InstanceXform();
	const CRhinoObjectAttributes attr = pBlock->Attributes();
	const CLBPString sName = attr.m_name;

	if (!doc.DeleteObject(CRhinoObjRef(pBlock)))
		return NULL;

	CRhinoInstanceDefinitionTable& def_table = doc.m_instance_definition_table;
	def_table.DeleteInstanceDefinition(iInstanceDefintionId, false, true);

	CRhinoInstanceObject* pAddedObject = AddToDocument(doc, sName, xformInstance);
	return pAddedObject;
}

CRhinoInstanceObject* CRpcInstance::AddToDocument(CRhinoDoc& doc, const ON_3dPoint& pt)
{
    ON_Xform xform = ON_Xform::TranslationTransformation(pt - ON_origin);

	const CLBPString sName = UnusedRpcName(RpcName());

	return AddToDocument(doc, sName, xform);
}

const CRhinoObject* CRpcInstance::Object(void) const
{
	const CRhinoDoc* pDoc = CRhinoDoc::FromRuntimeSerialNumber(m_idDoc);
	if (NULL != pDoc)
	{
		return pDoc->LookupObject(m_idObject);
	}
	return NULL;
}

CRhinoInstanceObject* CRpcInstance::AddToDocument(CRhinoDoc& doc, const CLBPString& sName,
												  const ON_Xform& xform)
{
	if (NULL == m_pInstance)
		return NULL;

	RPCapi::Mesh* pRpcMesh = m_pInstance->getEditMesh();
	if (NULL == pRpcMesh)
	{
		// const CLBPString sRpc = rpc.Name();
		// const CLBPString sMsg = L"RPC: " + sRpc + L" has no mesh. Invalid RPC.\n";
		RhinoApp().Print(_RhLocalizeString( L"RPC invalid: selected RPC has no mesh.\n", 36080));
		return NULL;
	}
	
	const double dUnitsScale = ON::UnitScale(ON::LengthUnitSystem::Inches, doc.ModelUnits());
	ON_Xform xformUnitsScale = ON_Xform::DiagonalTransformation(dUnitsScale, dUnitsScale, dUnitsScale);

	ON_Mesh* pRhinoMesh = new  ON_Mesh;

	RpcMesh2RhinoMesh(*pRpcMesh, *pRhinoMesh);
	
	delete pRpcMesh;

	pRhinoMesh->Transform(xformUnitsScale);

	CRhinoMeshObject* pMesh = new CRhinoMeshObject;
	pMesh->SetMesh(pRhinoMesh);

	CRhinoInstanceDefinitionTable& def_table = doc.m_instance_definition_table;

	const ON_wString sDefName = UnusedInstanceDefinitionName(doc).Wide();

	ON_InstanceDefinition idef;
	idef.SetName(sDefName);

	const int iIndex = def_table.AddInstanceDefinition(idef, pMesh);
    
    CRhinoInstanceObject* pInstObj = doc.m_instance_definition_table.AddInstanceObject(iIndex, xform);
	if (NULL != pInstObj)
	{
		CRhinoObjectAttributes attr = pInstObj->Attributes();
		attr.m_name = sName.Wide();
		pInstObj->ModifyAttributes(attr);

		CopyFromRpc(*pInstObj);
		m_idObject = pInstObj->ModelObjectId();
		m_idDoc = doc.RuntimeSerialNumber();
	}

	return pInstObj;
}
