

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 12:14:07 2038
 */
/* Compiler settings for COMNostrNative.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0628 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __COMNostrNative_i_h__
#define __COMNostrNative_i_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if defined(_CONTROL_FLOW_GUARD_XFG)
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

/* Forward Declarations */ 

#ifndef __INostrEventCallback_FWD_DEFINED__
#define __INostrEventCallback_FWD_DEFINED__
typedef interface INostrEventCallback INostrEventCallback;

#endif 	/* __INostrEventCallback_FWD_DEFINED__ */


#ifndef __INostrAuthCallback_FWD_DEFINED__
#define __INostrAuthCallback_FWD_DEFINED__
typedef interface INostrAuthCallback INostrAuthCallback;

#endif 	/* __INostrAuthCallback_FWD_DEFINED__ */


#ifndef __INostrSigner_FWD_DEFINED__
#define __INostrSigner_FWD_DEFINED__
typedef interface INostrSigner INostrSigner;

#endif 	/* __INostrSigner_FWD_DEFINED__ */


#ifndef __INostrRelaySession_FWD_DEFINED__
#define __INostrRelaySession_FWD_DEFINED__
typedef interface INostrRelaySession INostrRelaySession;

#endif 	/* __INostrRelaySession_FWD_DEFINED__ */


#ifndef __INostrSubscription_FWD_DEFINED__
#define __INostrSubscription_FWD_DEFINED__
typedef interface INostrSubscription INostrSubscription;

#endif 	/* __INostrSubscription_FWD_DEFINED__ */


#ifndef __INostrClient_FWD_DEFINED__
#define __INostrClient_FWD_DEFINED__
typedef interface INostrClient INostrClient;

#endif 	/* __INostrClient_FWD_DEFINED__ */


#ifndef __NostrClient_FWD_DEFINED__
#define __NostrClient_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrClient NostrClient;
#else
typedef struct NostrClient NostrClient;
#endif /* __cplusplus */

#endif 	/* __NostrClient_FWD_DEFINED__ */


#ifndef __NostrRelaySession_FWD_DEFINED__
#define __NostrRelaySession_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrRelaySession NostrRelaySession;
#else
typedef struct NostrRelaySession NostrRelaySession;
#endif /* __cplusplus */

#endif 	/* __NostrRelaySession_FWD_DEFINED__ */


#ifndef __NostrSubscription_FWD_DEFINED__
#define __NostrSubscription_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrSubscription NostrSubscription;
#else
typedef struct NostrSubscription NostrSubscription;
#endif /* __cplusplus */

#endif 	/* __NostrSubscription_FWD_DEFINED__ */


#ifndef __NostrSigner_FWD_DEFINED__
#define __NostrSigner_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrSigner NostrSigner;
#else
typedef struct NostrSigner NostrSigner;
#endif /* __cplusplus */

#endif 	/* __NostrSigner_FWD_DEFINED__ */


#ifndef __NostrEvent_FWD_DEFINED__
#define __NostrEvent_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrEvent NostrEvent;
#else
typedef struct NostrEvent NostrEvent;
#endif /* __cplusplus */

#endif 	/* __NostrEvent_FWD_DEFINED__ */


#ifndef __NostrFilter_FWD_DEFINED__
#define __NostrFilter_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrFilter NostrFilter;
#else
typedef struct NostrFilter NostrFilter;
#endif /* __cplusplus */

#endif 	/* __NostrFilter_FWD_DEFINED__ */


#ifndef __NostrTagQuery_FWD_DEFINED__
#define __NostrTagQuery_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrTagQuery NostrTagQuery;
#else
typedef struct NostrTagQuery NostrTagQuery;
#endif /* __cplusplus */

#endif 	/* __NostrTagQuery_FWD_DEFINED__ */


#ifndef __RelayDescriptor_FWD_DEFINED__
#define __RelayDescriptor_FWD_DEFINED__

#ifdef __cplusplus
typedef class RelayDescriptor RelayDescriptor;
#else
typedef struct RelayDescriptor RelayDescriptor;
#endif /* __cplusplus */

#endif 	/* __RelayDescriptor_FWD_DEFINED__ */


#ifndef __SubscriptionOptions_FWD_DEFINED__
#define __SubscriptionOptions_FWD_DEFINED__

#ifdef __cplusplus
typedef class SubscriptionOptions SubscriptionOptions;
#else
typedef struct SubscriptionOptions SubscriptionOptions;
#endif /* __cplusplus */

#endif 	/* __SubscriptionOptions_FWD_DEFINED__ */


#ifndef __AuthChallenge_FWD_DEFINED__
#define __AuthChallenge_FWD_DEFINED__

#ifdef __cplusplus
typedef class AuthChallenge AuthChallenge;
#else
typedef struct AuthChallenge AuthChallenge;
#endif /* __cplusplus */

#endif 	/* __AuthChallenge_FWD_DEFINED__ */


#ifndef __NostrEventDraft_FWD_DEFINED__
#define __NostrEventDraft_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrEventDraft NostrEventDraft;
#else
typedef struct NostrEventDraft NostrEventDraft;
#endif /* __cplusplus */

#endif 	/* __NostrEventDraft_FWD_DEFINED__ */


#ifndef __ClientOptions_FWD_DEFINED__
#define __ClientOptions_FWD_DEFINED__

#ifdef __cplusplus
typedef class ClientOptions ClientOptions;
#else
typedef struct ClientOptions ClientOptions;
#endif /* __cplusplus */

#endif 	/* __ClientOptions_FWD_DEFINED__ */


#ifndef __NostrOkResult_FWD_DEFINED__
#define __NostrOkResult_FWD_DEFINED__

#ifdef __cplusplus
typedef class NostrOkResult NostrOkResult;
#else
typedef struct NostrOkResult NostrOkResult;
#endif /* __cplusplus */

#endif 	/* __NostrOkResult_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __COMNostrNativeLib_LIBRARY_DEFINED__
#define __COMNostrNativeLib_LIBRARY_DEFINED__

/* library COMNostrNativeLib */
/* [helpstring][version][uuid] */ 

typedef /* [v1_enum] */ 
enum RelaySessionState
    {
        RelaySessionState_Disconnected	= 0,
        RelaySessionState_Connecting	= 1,
        RelaySessionState_Connected	= 2,
        RelaySessionState_Faulted	= 3
    } 	RelaySessionState;

typedef /* [v1_enum] */ 
enum SubscriptionStatus
    {
        SubscriptionStatus_Pending	= 0,
        SubscriptionStatus_Active	= 1,
        SubscriptionStatus_Draining	= 2,
        SubscriptionStatus_Closed	= 3
    } 	SubscriptionStatus;

typedef /* [v1_enum] */ 
enum QueueOverflowStrategy
    {
        QueueOverflowStrategy_DropOldest	= 0,
        QueueOverflowStrategy_Throw	= 1
    } 	QueueOverflowStrategy;








EXTERN_C const IID LIBID_COMNostrNativeLib;

#ifndef __INostrEventCallback_DISPINTERFACE_DEFINED__
#define __INostrEventCallback_DISPINTERFACE_DEFINED__

/* dispinterface INostrEventCallback */
/* [uuid] */ 


EXTERN_C const IID DIID_INostrEventCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("fa6b2b97-da84-47d7-9395-3be07d18bb8a")
    INostrEventCallback : public IDispatch
    {
    };
    
#else 	/* C style interface */

    typedef struct INostrEventCallbackVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            INostrEventCallback * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            INostrEventCallback * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            INostrEventCallback * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            INostrEventCallback * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            INostrEventCallback * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            INostrEventCallback * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            INostrEventCallback * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        END_INTERFACE
    } INostrEventCallbackVtbl;

    interface INostrEventCallback
    {
        CONST_VTBL struct INostrEventCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define INostrEventCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define INostrEventCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define INostrEventCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define INostrEventCallback_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define INostrEventCallback_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define INostrEventCallback_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define INostrEventCallback_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */


#endif 	/* __INostrEventCallback_DISPINTERFACE_DEFINED__ */


#ifndef __INostrAuthCallback_DISPINTERFACE_DEFINED__
#define __INostrAuthCallback_DISPINTERFACE_DEFINED__

/* dispinterface INostrAuthCallback */
/* [uuid] */ 


EXTERN_C const IID DIID_INostrAuthCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("0168a3e9-0da9-4fbe-92c7-6fb10c12c536")
    INostrAuthCallback : public IDispatch
    {
    };
    
#else 	/* C style interface */

    typedef struct INostrAuthCallbackVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            INostrAuthCallback * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            INostrAuthCallback * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            INostrAuthCallback * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            INostrAuthCallback * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            INostrAuthCallback * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            INostrAuthCallback * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            INostrAuthCallback * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        END_INTERFACE
    } INostrAuthCallbackVtbl;

    interface INostrAuthCallback
    {
        CONST_VTBL struct INostrAuthCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define INostrAuthCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define INostrAuthCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define INostrAuthCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define INostrAuthCallback_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define INostrAuthCallback_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define INostrAuthCallback_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define INostrAuthCallback_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */


#endif 	/* __INostrAuthCallback_DISPINTERFACE_DEFINED__ */


#ifndef __INostrSigner_DISPINTERFACE_DEFINED__
#define __INostrSigner_DISPINTERFACE_DEFINED__

/* dispinterface INostrSigner */
/* [uuid] */ 


EXTERN_C const IID DIID_INostrSigner;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("0c458d3c-2e65-4a90-9f64-7809e944adbc")
    INostrSigner : public IDispatch
    {
    };
    
#else 	/* C style interface */

    typedef struct INostrSignerVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            INostrSigner * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            INostrSigner * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            INostrSigner * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            INostrSigner * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            INostrSigner * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            INostrSigner * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            INostrSigner * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        END_INTERFACE
    } INostrSignerVtbl;

    interface INostrSigner
    {
        CONST_VTBL struct INostrSignerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define INostrSigner_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define INostrSigner_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define INostrSigner_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define INostrSigner_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define INostrSigner_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define INostrSigner_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define INostrSigner_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */


#endif 	/* __INostrSigner_DISPINTERFACE_DEFINED__ */


#ifndef __INostrRelaySession_INTERFACE_DEFINED__
#define __INostrRelaySession_INTERFACE_DEFINED__

/* interface INostrRelaySession */
/* [unique][oleautomation][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_INostrRelaySession;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2d375c22-fda2-4286-bcbe-81fdf2f245b5")
    INostrRelaySession : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Url( 
            /* [retval][out] */ BSTR *value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_State( 
            /* [retval][out] */ RelaySessionState *value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_LastOkResult( 
            /* [retval][out] */ IDispatch **value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_SupportedNips( 
            /* [retval][out] */ SAFEARRAY * *value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_WriteEnabled( 
            /* [retval][out] */ VARIANT_BOOL *value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_ReadEnabled( 
            /* [retval][out] */ VARIANT_BOOL *value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE Reconnect( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE Close( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE GetDescriptor( 
            /* [retval][out] */ IDispatch **value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE UpdatePolicy( 
            /* [in] */ IDispatch *descriptor) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct INostrRelaySessionVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            INostrRelaySession * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            INostrRelaySession * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            INostrRelaySession * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            INostrRelaySession * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            INostrRelaySession * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            INostrRelaySession * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            INostrRelaySession * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, get_Url)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Url )( 
            INostrRelaySession * This,
            /* [retval][out] */ BSTR *value);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, get_State)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_State )( 
            INostrRelaySession * This,
            /* [retval][out] */ RelaySessionState *value);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, get_LastOkResult)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_LastOkResult )( 
            INostrRelaySession * This,
            /* [retval][out] */ IDispatch **value);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, get_SupportedNips)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_SupportedNips )( 
            INostrRelaySession * This,
            /* [retval][out] */ SAFEARRAY * *value);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, get_WriteEnabled)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_WriteEnabled )( 
            INostrRelaySession * This,
            /* [retval][out] */ VARIANT_BOOL *value);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, get_ReadEnabled)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_ReadEnabled )( 
            INostrRelaySession * This,
            /* [retval][out] */ VARIANT_BOOL *value);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, Reconnect)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *Reconnect )( 
            INostrRelaySession * This);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, Close)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *Close )( 
            INostrRelaySession * This);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, GetDescriptor)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *GetDescriptor )( 
            INostrRelaySession * This,
            /* [retval][out] */ IDispatch **value);
        
        DECLSPEC_XFGVIRT(INostrRelaySession, UpdatePolicy)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *UpdatePolicy )( 
            INostrRelaySession * This,
            /* [in] */ IDispatch *descriptor);
        
        END_INTERFACE
    } INostrRelaySessionVtbl;

    interface INostrRelaySession
    {
        CONST_VTBL struct INostrRelaySessionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define INostrRelaySession_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define INostrRelaySession_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define INostrRelaySession_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define INostrRelaySession_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define INostrRelaySession_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define INostrRelaySession_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define INostrRelaySession_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define INostrRelaySession_get_Url(This,value)	\
    ( (This)->lpVtbl -> get_Url(This,value) ) 

#define INostrRelaySession_get_State(This,value)	\
    ( (This)->lpVtbl -> get_State(This,value) ) 

#define INostrRelaySession_get_LastOkResult(This,value)	\
    ( (This)->lpVtbl -> get_LastOkResult(This,value) ) 

#define INostrRelaySession_get_SupportedNips(This,value)	\
    ( (This)->lpVtbl -> get_SupportedNips(This,value) ) 

#define INostrRelaySession_get_WriteEnabled(This,value)	\
    ( (This)->lpVtbl -> get_WriteEnabled(This,value) ) 

#define INostrRelaySession_get_ReadEnabled(This,value)	\
    ( (This)->lpVtbl -> get_ReadEnabled(This,value) ) 

#define INostrRelaySession_Reconnect(This)	\
    ( (This)->lpVtbl -> Reconnect(This) ) 

#define INostrRelaySession_Close(This)	\
    ( (This)->lpVtbl -> Close(This) ) 

#define INostrRelaySession_GetDescriptor(This,value)	\
    ( (This)->lpVtbl -> GetDescriptor(This,value) ) 

#define INostrRelaySession_UpdatePolicy(This,descriptor)	\
    ( (This)->lpVtbl -> UpdatePolicy(This,descriptor) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __INostrRelaySession_INTERFACE_DEFINED__ */


#ifndef __INostrSubscription_INTERFACE_DEFINED__
#define __INostrSubscription_INTERFACE_DEFINED__

/* interface INostrSubscription */
/* [unique][oleautomation][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_INostrSubscription;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("143205c5-f229-4e2e-a47c-25c34a7f040d")
    INostrSubscription : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Id( 
            /* [retval][out] */ BSTR *value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Status( 
            /* [retval][out] */ SubscriptionStatus *value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Filters( 
            /* [retval][out] */ SAFEARRAY * *value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE UpdateFilters( 
            /* [in] */ SAFEARRAY * filters) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE Close( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct INostrSubscriptionVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            INostrSubscription * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            INostrSubscription * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            INostrSubscription * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            INostrSubscription * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            INostrSubscription * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            INostrSubscription * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            INostrSubscription * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(INostrSubscription, get_Id)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Id )( 
            INostrSubscription * This,
            /* [retval][out] */ BSTR *value);
        
        DECLSPEC_XFGVIRT(INostrSubscription, get_Status)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Status )( 
            INostrSubscription * This,
            /* [retval][out] */ SubscriptionStatus *value);
        
        DECLSPEC_XFGVIRT(INostrSubscription, get_Filters)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Filters )( 
            INostrSubscription * This,
            /* [retval][out] */ SAFEARRAY * *value);
        
        DECLSPEC_XFGVIRT(INostrSubscription, UpdateFilters)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *UpdateFilters )( 
            INostrSubscription * This,
            /* [in] */ SAFEARRAY * filters);
        
        DECLSPEC_XFGVIRT(INostrSubscription, Close)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *Close )( 
            INostrSubscription * This);
        
        END_INTERFACE
    } INostrSubscriptionVtbl;

    interface INostrSubscription
    {
        CONST_VTBL struct INostrSubscriptionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define INostrSubscription_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define INostrSubscription_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define INostrSubscription_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define INostrSubscription_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define INostrSubscription_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define INostrSubscription_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define INostrSubscription_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define INostrSubscription_get_Id(This,value)	\
    ( (This)->lpVtbl -> get_Id(This,value) ) 

#define INostrSubscription_get_Status(This,value)	\
    ( (This)->lpVtbl -> get_Status(This,value) ) 

#define INostrSubscription_get_Filters(This,value)	\
    ( (This)->lpVtbl -> get_Filters(This,value) ) 

#define INostrSubscription_UpdateFilters(This,filters)	\
    ( (This)->lpVtbl -> UpdateFilters(This,filters) ) 

#define INostrSubscription_Close(This)	\
    ( (This)->lpVtbl -> Close(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __INostrSubscription_INTERFACE_DEFINED__ */


#ifndef __INostrClient_INTERFACE_DEFINED__
#define __INostrClient_INTERFACE_DEFINED__

/* interface INostrClient */
/* [unique][oleautomation][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_INostrClient;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("75f24149-8cbf-4f9c-9482-ec8374fdc7b5")
    INostrClient : public IDispatch
    {
    public:
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE Initialize( 
            /* [in] */ IDispatch *options) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE SetSigner( 
            /* [in] */ INostrSigner	*signer) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE ConnectRelay( 
            /* [in] */ IDispatch *descriptor,
            /* [in] */ INostrAuthCallback	*authCallback,
            /* [retval][out] */ INostrRelaySession **session) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE DisconnectRelay( 
            /* [in] */ BSTR relayUrl) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE HasRelay( 
            /* [in] */ BSTR relayUrl,
            /* [retval][out] */ VARIANT_BOOL *hasRelay) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE OpenSubscription( 
            /* [in] */ BSTR relayUrl,
            /* [in] */ SAFEARRAY * filters,
            /* [in] */ INostrEventCallback	*callback,
            /* [in] */ IDispatch *options,
            /* [retval][out] */ INostrSubscription **subscription) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE PublishEvent( 
            /* [in] */ BSTR relayUrl,
            /* [in] */ IDispatch *eventPayload) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE RespondAuth( 
            /* [in] */ BSTR relayUrl,
            /* [in] */ IDispatch *authEvent) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE RefreshRelayInfo( 
            /* [in] */ BSTR relayUrl) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE ListRelays( 
            /* [retval][out] */ SAFEARRAY * *relayUrls) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct INostrClientVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            INostrClient * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            INostrClient * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            INostrClient * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            INostrClient * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            INostrClient * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            INostrClient * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            INostrClient * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(INostrClient, Initialize)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            INostrClient * This,
            /* [in] */ IDispatch *options);
        
        DECLSPEC_XFGVIRT(INostrClient, SetSigner)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *SetSigner )( 
            INostrClient * This,
            /* [in] */ INostrSigner	*signer);
        
        DECLSPEC_XFGVIRT(INostrClient, ConnectRelay)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *ConnectRelay )( 
            INostrClient * This,
            /* [in] */ IDispatch *descriptor,
            /* [in] */ INostrAuthCallback	*authCallback,
            /* [retval][out] */ INostrRelaySession **session);
        
        DECLSPEC_XFGVIRT(INostrClient, DisconnectRelay)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *DisconnectRelay )( 
            INostrClient * This,
            /* [in] */ BSTR relayUrl);
        
        DECLSPEC_XFGVIRT(INostrClient, HasRelay)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *HasRelay )( 
            INostrClient * This,
            /* [in] */ BSTR relayUrl,
            /* [retval][out] */ VARIANT_BOOL *hasRelay);
        
        DECLSPEC_XFGVIRT(INostrClient, OpenSubscription)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *OpenSubscription )( 
            INostrClient * This,
            /* [in] */ BSTR relayUrl,
            /* [in] */ SAFEARRAY * filters,
            /* [in] */ INostrEventCallback	*callback,
            /* [in] */ IDispatch *options,
            /* [retval][out] */ INostrSubscription **subscription);
        
        DECLSPEC_XFGVIRT(INostrClient, PublishEvent)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *PublishEvent )( 
            INostrClient * This,
            /* [in] */ BSTR relayUrl,
            /* [in] */ IDispatch *eventPayload);
        
        DECLSPEC_XFGVIRT(INostrClient, RespondAuth)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *RespondAuth )( 
            INostrClient * This,
            /* [in] */ BSTR relayUrl,
            /* [in] */ IDispatch *authEvent);
        
        DECLSPEC_XFGVIRT(INostrClient, RefreshRelayInfo)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *RefreshRelayInfo )( 
            INostrClient * This,
            /* [in] */ BSTR relayUrl);
        
        DECLSPEC_XFGVIRT(INostrClient, ListRelays)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *ListRelays )( 
            INostrClient * This,
            /* [retval][out] */ SAFEARRAY * *relayUrls);
        
        END_INTERFACE
    } INostrClientVtbl;

    interface INostrClient
    {
        CONST_VTBL struct INostrClientVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define INostrClient_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define INostrClient_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define INostrClient_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define INostrClient_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define INostrClient_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define INostrClient_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define INostrClient_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define INostrClient_Initialize(This,options)	\
    ( (This)->lpVtbl -> Initialize(This,options) ) 

#define INostrClient_SetSigner(This,signer)	\
    ( (This)->lpVtbl -> SetSigner(This,signer) ) 

#define INostrClient_ConnectRelay(This,descriptor,authCallback,session)	\
    ( (This)->lpVtbl -> ConnectRelay(This,descriptor,authCallback,session) ) 

#define INostrClient_DisconnectRelay(This,relayUrl)	\
    ( (This)->lpVtbl -> DisconnectRelay(This,relayUrl) ) 

#define INostrClient_HasRelay(This,relayUrl,hasRelay)	\
    ( (This)->lpVtbl -> HasRelay(This,relayUrl,hasRelay) ) 

#define INostrClient_OpenSubscription(This,relayUrl,filters,callback,options,subscription)	\
    ( (This)->lpVtbl -> OpenSubscription(This,relayUrl,filters,callback,options,subscription) ) 

#define INostrClient_PublishEvent(This,relayUrl,eventPayload)	\
    ( (This)->lpVtbl -> PublishEvent(This,relayUrl,eventPayload) ) 

#define INostrClient_RespondAuth(This,relayUrl,authEvent)	\
    ( (This)->lpVtbl -> RespondAuth(This,relayUrl,authEvent) ) 

#define INostrClient_RefreshRelayInfo(This,relayUrl)	\
    ( (This)->lpVtbl -> RefreshRelayInfo(This,relayUrl) ) 

#define INostrClient_ListRelays(This,relayUrls)	\
    ( (This)->lpVtbl -> ListRelays(This,relayUrls) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __INostrClient_INTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_NostrClient;

#ifdef __cplusplus

class DECLSPEC_UUID("7d3091fe-ca18-49ba-835c-012991076660")
NostrClient;
#endif

EXTERN_C const CLSID CLSID_NostrRelaySession;

#ifdef __cplusplus

class DECLSPEC_UUID("e53e9b56-da8d-4064-8df6-5563708f65a5")
NostrRelaySession;
#endif

EXTERN_C const CLSID CLSID_NostrSubscription;

#ifdef __cplusplus

class DECLSPEC_UUID("175bd625-18d9-42bd-b75a-0642abf029b4")
NostrSubscription;
#endif

EXTERN_C const CLSID CLSID_NostrSigner;

#ifdef __cplusplus

class DECLSPEC_UUID("ae9df2b5-8650-4a51-8bb2-1df35a48a6ec")
NostrSigner;
#endif

EXTERN_C const CLSID CLSID_NostrEvent;

#ifdef __cplusplus

class DECLSPEC_UUID("56e23422-5e99-4be6-a29d-fb60eccb6559")
NostrEvent;
#endif

EXTERN_C const CLSID CLSID_NostrFilter;

#ifdef __cplusplus

class DECLSPEC_UUID("ed57aa18-b9fe-4861-a6e9-66d68b9a0c49")
NostrFilter;
#endif

EXTERN_C const CLSID CLSID_NostrTagQuery;

#ifdef __cplusplus

class DECLSPEC_UUID("4ed88857-740f-451e-8d1f-5959c89f31a2")
NostrTagQuery;
#endif

EXTERN_C const CLSID CLSID_RelayDescriptor;

#ifdef __cplusplus

class DECLSPEC_UUID("43910586-5980-4cbc-8936-ea8d1d2cb584")
RelayDescriptor;
#endif

EXTERN_C const CLSID CLSID_SubscriptionOptions;

#ifdef __cplusplus

class DECLSPEC_UUID("b8522064-312e-4ce0-b4c5-e7d59a76d073")
SubscriptionOptions;
#endif

EXTERN_C const CLSID CLSID_AuthChallenge;

#ifdef __cplusplus

class DECLSPEC_UUID("83822390-3a93-4edb-ba46-2f2dc960d08d")
AuthChallenge;
#endif

EXTERN_C const CLSID CLSID_NostrEventDraft;

#ifdef __cplusplus

class DECLSPEC_UUID("38c8b451-af80-41d6-ae98-36ab672412e7")
NostrEventDraft;
#endif

EXTERN_C const CLSID CLSID_ClientOptions;

#ifdef __cplusplus

class DECLSPEC_UUID("21831b74-c106-4809-b47d-1bec57addc7c")
ClientOptions;
#endif

EXTERN_C const CLSID CLSID_NostrOkResult;

#ifdef __cplusplus

class DECLSPEC_UUID("9b7fce0f-14c5-43ec-97f0-b968f479f3a1")
NostrOkResult;
#endif
#endif /* __COMNostrNativeLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


