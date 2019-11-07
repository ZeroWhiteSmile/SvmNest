#include "BaseUtil.h"
#include "SvmUtil.h"

VOID SetvCpuMode(PVIRTUAL_PROCESSOR_DATA pVpdata, CPU_MODE CpuMode)
{
    //guest_context->stack->processor_data->CpuMode = CpuMode;
    pVpdata->HostStackLayout.pProcessNestData->CpuMode = CpuMode;
}

// VA -> PA
ULONG64 UtilPaFromVa(void *va)
{
    const auto pa = MmGetPhysicalAddress(va);
    return pa.QuadPart;
}

// PA -> VA
void *UtilVaFromPa(ULONG64 pa) 
{
    PHYSICAL_ADDRESS pa2 = {};
    pa2.QuadPart = pa;
    return MmGetVirtualForPhysical(pa2);
}

void SaveHostKernelGsBase(PVIRTUAL_PROCESSOR_DATA pVpdata)
{
	//vcpu->HostKernelGsBase.QuadPart = UtilReadMsr64(Msr::kIa32KernelGsBase);
	pVpdata->HostStackLayout.pProcessNestData->HostKernelGsBase.QuadPart = UtilReadMsr64(Msr::kIa32KernelGsBase);
}

//------------------------------------------------------------------------------------------------//
VOID
ENTER_GUEST_MODE(
    _In_	VCPUVMX* vm
)
/*++

Desscription:

    Virtual process enter the Guest Mode.

Paremeters:

    Guest Context

Return Value:

    NO

--*/
{
    vm->inRoot = GuestMode;
    SvDebugPrint("VMM: %I64x Enter Guest mode", vm);
}

//------------------------------------------------------------------------------------------------//
VOID
LEAVE_GUEST_MODE(
    _In_	VCPUVMX* vm
)
/*++

Desscription:

    Virtual process enter the Root Mode.

Paremeters:

    Guest Context

Return Value:

    NO

--*/
{
    vm->inRoot = RootMode;
    //HYPERPLATFORM_LOG_DEBUG("VMM: %I64x Enter Root mode Reason: %d", vm, UtilVmRead(VmcsField::kVmExitReason));
    SvDebugPrint("VMM: %I64x Enter Root mode Reason: ", vm);
}

//------------------------------------------------------------------------------------------------//
VMX_MODE
VmxGetVmxMode(
    _In_ VCPUVMX* vmx
)
/*++

Desscription:

    Get VMX Mode of the corresponding virtual processor

Paremeters:

    Guest Context

Return Value:

    Emulated-Root or Emulated-Guest Mode

--*/
{
    if (vmx)
    {
        return vmx->inRoot;
    }
    else
    {
        return VMX_MODE::RootMode;
    }
}

//----------------------------------------------------------------------------------------------------------------//
VCPUVMX* VmmpGetVcpuVmx(PVIRTUAL_PROCESSOR_DATA pVpdata)
{
    //return guest_context->stack->processor_data->vcpu_vmx;
    return pVpdata->HostStackLayout.pProcessNestData->vcpu_vmx;
}

VOID SimulateReloadHostStateInVmcbGuest02(_Inout_ PVIRTUAL_PROCESSOR_DATA VpData, _Inout_ PGUEST_CONTEXT GuestContext)
{
    //reload host state
    PVMCB pVmcbGuest12va = GetCurrentVmcbGuest12(VpData);
    PVMCB pVmcbGuest02va = GetCurrentVmcbGuest02(VpData);

    GuestContext->VpRegs->Rax = VmmpGetVcpuVmx(VpData)->vmcb_guest_12_pa; //  L2 rax, vmcb12pa
    pVmcbGuest02va->StateSaveArea.Rsp = VpData->GuestVmcb.StateSaveArea.Rsp; // L2 host rsp 
    pVmcbGuest02va->StateSaveArea.Rip = VpData->GuestVmcb.ControlArea.NRip; // L2 host ip 

    // the Rflags is in vmcb01 when ret to L1 host first , but it is in vmcb02 later on. i think that is all right
    //pVmcbGuest02va->StateSaveArea.Rflags = VpData->GuestVmcb.StateSaveArea.Rflags; // not right , but can not find

    SvDebugPrint("[SaveGuestVmcb12FromGuestVmcb02] pVmcbGuest12va->StateSaveArea.Rax  : %I64X \r\n", pVmcbGuest12va->StateSaveArea.Rax);
    SvDebugPrint("[SaveGuestVmcb12FromGuestVmcb02] pVmcbGuest12va->StateSaveArea.Rsp  : %I64X \r\n", pVmcbGuest12va->StateSaveArea.Rsp);
    SvDebugPrint("[SaveGuestVmcb12FromGuestVmcb02] pVmcbGuest12va->StateSaveArea.Rip  : %I64X \r\n", pVmcbGuest12va->StateSaveArea.Rip);
    SvDebugPrint("[SaveGuestVmcb12FromGuestVmcb02] pVmcbGuest12va->ControlArea.NRip  : %I64X \r\n", pVmcbGuest12va->ControlArea.NRip);
    SvDebugPrint("[SaveGuestVmcb12FromGuestVmcb02] GuestContext->VpRegs->Rax  : %I64X \r\n", GuestContext->VpRegs->Rax);
    SvDebugPrint("[SaveGuestVmcb12FromGuestVmcb02] pVmcbGuest02va->StateSaveArea.Rsp  : %I64X \r\n", pVmcbGuest02va->StateSaveArea.Rsp);
    SvDebugPrint("[SaveGuestVmcb12FromGuestVmcb02] pVmcbGuest02va->StateSaveArea.Rip  : %I64X \r\n", pVmcbGuest02va->StateSaveArea.Rip);

}

VOID SaveGuestVmcb12FromGuestVmcb02(_Inout_ PVIRTUAL_PROCESSOR_DATA VpData, _Inout_ PGUEST_CONTEXT GuestContext)
{
    PVMCB pVmcbGuest02va = (PVMCB)UtilVaFromPa(VpData->HostStackLayout.pProcessNestData->vcpu_vmx->vmcb_guest_02_pa);
    PVMCB pVmcbGuest12va = (PVMCB)UtilVaFromPa(VpData->HostStackLayout.pProcessNestData->vcpu_vmx->vmcb_guest_12_pa);

    pVmcbGuest12va->StateSaveArea.Rax = GuestContext->VpRegs->Rax; // save L2 rax => vmcb12 in L2
    pVmcbGuest12va->StateSaveArea.Rsp = pVmcbGuest02va->StateSaveArea.Rsp; // save L2 guest rsp=> vmcb12
    pVmcbGuest12va->StateSaveArea.Rflags = pVmcbGuest02va->StateSaveArea.Rflags; // save L2 guest rflags => vmcb12
    pVmcbGuest12va->StateSaveArea.Rip = pVmcbGuest02va->StateSaveArea.Rip; // save L2 rip => vmcb12
    pVmcbGuest12va->ControlArea.NRip = pVmcbGuest02va->ControlArea.NRip; // save L2 next rip => vmcb12

    pVmcbGuest12va->ControlArea.ExitCode = pVmcbGuest02va->ControlArea.ExitCode;
    pVmcbGuest12va->ControlArea.ExitInfo1 = pVmcbGuest02va->ControlArea.ExitInfo1;
    pVmcbGuest12va->ControlArea.ExitInfo2 = pVmcbGuest02va->ControlArea.ExitInfo2;
    pVmcbGuest12va->ControlArea.ExitIntInfo = pVmcbGuest02va->ControlArea.ExitIntInfo;
    pVmcbGuest12va->ControlArea.EventInj = pVmcbGuest02va->ControlArea.EventInj;
    pVmcbGuest12va->StateSaveArea.Cpl = pVmcbGuest02va->StateSaveArea.Cpl;
    pVmcbGuest12va->StateSaveArea.LStar = pVmcbGuest02va->StateSaveArea.LStar;

    // simulate save guest state to VMCB12: 

    // ES.{base,limit,attr,sel} 
    // CS.{base,limit,attr,sel} 
    // SS.{base,limit,attr,sel} 
    // DS.{base,limit,attr,sel} 
    pVmcbGuest12va->StateSaveArea.CsLimit = pVmcbGuest02va->StateSaveArea.CsLimit;
    pVmcbGuest12va->StateSaveArea.DsLimit = pVmcbGuest02va->StateSaveArea.DsLimit;
    pVmcbGuest12va->StateSaveArea.EsLimit = pVmcbGuest02va->StateSaveArea.EsLimit;
    pVmcbGuest12va->StateSaveArea.SsLimit = pVmcbGuest02va->StateSaveArea.SsLimit;
    pVmcbGuest12va->StateSaveArea.CsSelector = pVmcbGuest02va->StateSaveArea.CsSelector;
    pVmcbGuest12va->StateSaveArea.DsSelector = pVmcbGuest02va->StateSaveArea.DsSelector;
    pVmcbGuest12va->StateSaveArea.EsSelector = pVmcbGuest02va->StateSaveArea.EsSelector;
    pVmcbGuest12va->StateSaveArea.SsSelector = pVmcbGuest02va->StateSaveArea.SsSelector;
    pVmcbGuest12va->StateSaveArea.CsAttrib = pVmcbGuest02va->StateSaveArea.CsAttrib;
    pVmcbGuest12va->StateSaveArea.DsAttrib = pVmcbGuest02va->StateSaveArea.DsAttrib;
    pVmcbGuest12va->StateSaveArea.EsAttrib = pVmcbGuest02va->StateSaveArea.EsAttrib;
    pVmcbGuest12va->StateSaveArea.SsAttrib = pVmcbGuest02va->StateSaveArea.SsAttrib;

    //GDTR.{base, limit}
    //IDTR.{base, limit}

    pVmcbGuest12va->StateSaveArea.GdtrBase = pVmcbGuest02va->StateSaveArea.GdtrBase;
    pVmcbGuest12va->StateSaveArea.GdtrLimit = pVmcbGuest02va->StateSaveArea.GdtrLimit;
    pVmcbGuest12va->StateSaveArea.IdtrBase = pVmcbGuest02va->StateSaveArea.IdtrBase;
    pVmcbGuest12va->StateSaveArea.IdtrLimit = pVmcbGuest02va->StateSaveArea.IdtrLimit;

    //EFER CR4 CR3 CR2 CR0
    pVmcbGuest12va->StateSaveArea.Efer = pVmcbGuest02va->StateSaveArea.Efer;
    pVmcbGuest12va->StateSaveArea.Cr4 = pVmcbGuest02va->StateSaveArea.Cr4;
    pVmcbGuest12va->StateSaveArea.Cr3 = pVmcbGuest02va->StateSaveArea.Cr3;
    pVmcbGuest12va->StateSaveArea.Cr2 = pVmcbGuest02va->StateSaveArea.Cr2;
    pVmcbGuest12va->StateSaveArea.Cr0 = pVmcbGuest02va->StateSaveArea.Cr0;

    //if (nested paging enabled)    gPAT
    pVmcbGuest12va->StateSaveArea.GPat = pVmcbGuest02va->StateSaveArea.GPat;

    // DR7 DR6 
    pVmcbGuest12va->StateSaveArea.Dr7 = pVmcbGuest02va->StateSaveArea.Dr7;
    pVmcbGuest12va->StateSaveArea.Dr6 = pVmcbGuest02va->StateSaveArea.Dr6;

}

VMCB * GetCurrentVmcbGuest12(PVIRTUAL_PROCESSOR_DATA pVpdata)
{
    PVMCB pVmcbGuest12va = (PVMCB)UtilVaFromPa(VmmpGetVcpuVmx(pVpdata)->vmcb_guest_12_pa);
    return pVmcbGuest12va;
}

VMCB * GetCurrentVmcbGuest02(PVIRTUAL_PROCESSOR_DATA pVpdata)
{
    PVMCB pVmcbGuest02va = (PVMCB)UtilVaFromPa(VmmpGetVcpuVmx(pVpdata)->vmcb_guest_02_pa);
    return pVmcbGuest02va;
}

VOID HandleMsrReadAndWrite(
_Inout_ PVIRTUAL_PROCESSOR_DATA VpData,
    _Inout_ PGUEST_CONTEXT GuestContext)
{
    PVMCB pVmcbGuest02va = GetCurrentVmcbGuest02(VpData);
    LARGE_INTEGER MsrValue = { 0 };
    if (0 == pVmcbGuest02va->ControlArea.ExitInfo1) // read
    {
        Msr MsrNum = (Msr)GuestContext->VpRegs->Rcx;
        MsrValue.QuadPart = UtilReadMsr64(MsrNum); // read from host

        GuestContext->VpRegs->Rax = MsrValue.LowPart;
        GuestContext->VpRegs->Rdx = MsrValue.HighPart;
    }
    else
    {
        Msr MsrNum = (Msr)GuestContext->VpRegs->Rcx;
        MsrValue.LowPart = (ULONG)GuestContext->VpRegs->Rax;
        MsrValue.HighPart = (ULONG)GuestContext->VpRegs->Rdx;
        UtilWriteMsr64(MsrNum, MsrValue.QuadPart);
    }
}

BOOLEAN CheckVmcb12MsrBit(
_Inout_ PVIRTUAL_PROCESSOR_DATA VpData,
    _Inout_ PGUEST_CONTEXT GuestContext)
{
    PVMCB pVmcbGuest12va = GetCurrentVmcbGuest12(VpData);
	PVMCB pVmcbGuest02va = GetCurrentVmcbGuest02(VpData);
	BOOL bIsWrite = (BOOL)pVmcbGuest02va->ControlArea.ExitInfo1;

    PVOID MsrPermissionsMap = UtilVaFromPa(pVmcbGuest12va->ControlArea.MsrpmBasePa);
    RTL_BITMAP bitmapHeader;
	static const UINT32 FIRST_MSR_RANGE_BASE = 0x00000000;
	static const UINT32 SECOND_MSR_RANGE_BASE = 0xc0000000;
	static const UINT32 THIRD_MSR_RANGE_BASE = 0xC0010000;
	static const UINT32 BITS_PER_MSR = 2;
	static const UINT32 FIRST_MSRPM_OFFSET = 0x000 * CHAR_BIT;
	static const UINT32 SECOND_MSRPM_OFFSET = 0x800 * CHAR_BIT;
	static const UINT32 THIRD_MSRPM_OFFSET = 0x1000 * CHAR_BIT;
	ULONG64 offsetFromBase = 0;
    ULONG64 offset = 0;

    RtlInitializeBitMap(&bitmapHeader,
        reinterpret_cast<PULONG>(MsrPermissionsMap),
        SVM_MSR_PERMISSIONS_MAP_SIZE * CHAR_BIT
    );

	UINT64 MsrNum = GuestContext->VpRegs->Rcx;
	if (MsrNum > FIRST_MSR_RANGE_BASE && MsrNum < SECOND_MSR_RANGE_BASE)
	{
		offsetFromBase = (MsrNum - FIRST_MSR_RANGE_BASE) * BITS_PER_MSR;
		offset = FIRST_MSRPM_OFFSET + offsetFromBase;
	}
	if (MsrNum > SECOND_MSR_RANGE_BASE && MsrNum < THIRD_MSR_RANGE_BASE)
	{
        offsetFromBase = (MsrNum - SECOND_MSR_RANGE_BASE) * BITS_PER_MSR;
        offset = SECOND_MSRPM_OFFSET + offsetFromBase;
	}
	if (MsrNum > THIRD_MSR_RANGE_BASE)
	{
        offsetFromBase = (MsrNum - THIRD_MSR_RANGE_BASE) * BITS_PER_MSR;
        offset = THIRD_MSRPM_OFFSET + offsetFromBase;
	}

    BOOLEAN bret = FALSE;
    if (FALSE == bIsWrite)
    {
        bret = RtlTestBit(&bitmapHeader, (ULONG)offset);
    }
    else
    {
        bret = RtlTestBit(&bitmapHeader, ULONG(offset + 1));
    }
    return bret;
}

void ClearVGIF(PVIRTUAL_PROCESSOR_DATA VpData)
{
    //60h 9 VGIF value(0 �C Virtual interrupts are masked, 1 �C Virtual Interrupts are unmasked)
    UINT64 tmp = ~GetCurrentVmcbGuest02(VpData)->ControlArea.VIntr;
    tmp |= (1UL << 9);
    GetCurrentVmcbGuest02(VpData)->ControlArea.VIntr = ~tmp;
}

void SetVGIF(PVIRTUAL_PROCESSOR_DATA VpData)
{
    //60h 9 VGIF value(0 �C Virtual interrupts are masked, 1 �C Virtual Interrupts are unmasked)
    GetCurrentVmcbGuest02(VpData)->ControlArea.VIntr |= (1UL << 9);
}

void LeaveGuest(
    _Inout_ PVIRTUAL_PROCESSOR_DATA VpData,
    _Inout_ PGUEST_CONTEXT GuestContext)
{
    // Clears the global interrupt flag (GIF). While GIF is zero, all external interrupts are disabled. 
    ClearVGIF(VpData);
    SaveGuestVmcb12FromGuestVmcb02(VpData, GuestContext);
    SimulateReloadHostStateInVmcbGuest02(VpData, GuestContext);
    LEAVE_GUEST_MODE(VmmpGetVcpuVmx(VpData));
}

void SimulateVmrun02SaveHostStateShadow(
    _Inout_ PVMCB pVmcb,
    _Inout_ PVIRTUAL_PROCESSOR_DATA VpData,
    _Inout_ PGUEST_CONTEXT GuestContext)
{
    // save host state to physical memory indicated in the VM_HSAVE_PA MSR: 

    PVMCB pVmcbHostStateShadow = &(VmmpGetVcpuVmx(VpData)->VmcbHostStateArea02Shadow);
    //ES.sel 
    //CS.sel 
    //SS.sel
    //DS.sel
    pVmcbHostStateShadow->StateSaveArea.CsSelector = pVmcb->StateSaveArea.CsSelector;
    pVmcbHostStateShadow->StateSaveArea.DsSelector = pVmcb->StateSaveArea.DsSelector;
    pVmcbHostStateShadow->StateSaveArea.EsSelector = pVmcb->StateSaveArea.EsSelector;
    pVmcbHostStateShadow->StateSaveArea.SsSelector = pVmcb->StateSaveArea.SsSelector;

    //GDTR.{base,limit} 
    //IDTR.{base,limit} 
    pVmcbHostStateShadow->StateSaveArea.GdtrBase = pVmcb->StateSaveArea.GdtrBase;
    pVmcbHostStateShadow->StateSaveArea.GdtrLimit = pVmcb->StateSaveArea.GdtrLimit;
    pVmcbHostStateShadow->StateSaveArea.IdtrBase = pVmcb->StateSaveArea.IdtrBase;
    pVmcbHostStateShadow->StateSaveArea.IdtrLimit = pVmcb->StateSaveArea.IdtrLimit;

    //EFER 
    //CR0 
    //CR4 
    //CR3
    pVmcbHostStateShadow->StateSaveArea.Efer = pVmcb->StateSaveArea.Efer;
    pVmcbHostStateShadow->StateSaveArea.Cr0 = pVmcb->StateSaveArea.Cr0;
    pVmcbHostStateShadow->StateSaveArea.Cr4 = pVmcb->StateSaveArea.Cr4;
    pVmcbHostStateShadow->StateSaveArea.Cr3 = pVmcb->StateSaveArea.Cr3;

    // host CR2 is not saved 
    //RFLAGS 
    pVmcbHostStateShadow->StateSaveArea.Rflags = pVmcb->StateSaveArea.Rflags;


}