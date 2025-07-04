# Intel Network Adapter WMI Driver Interface Analysis

This document summarizes the available WMI/CIM classes and methods exposed by Intel PROSet/driver stack (as found in the Wired_PROSet_30.1_x64 package) for use in Windows-specific driver interface development. This analysis is the basis for implementing a Windows-specific driver interface DLL for advanced adapter feature detection and configuration (e.g., hardware timestamping, PTP, diagnostics).

## WMI Namespace
- All Intel-specific classes are registered under the namespace: `root\IntelNCS2`

## Key Classes and Their Purpose

### 1. IANet_PhysicalEthernetAdapter
- **Description:** Represents a physical Intel Ethernet adapter.
- **Properties (as observed on Intel I210 with PROSet/WMI):**
  - `AdapterStatus`, `AdditionalAvailability`, `AlignmentErrors`, `AutoSense`, `Availability`, `BusType`, `BusTypeString`, `Capabilities`, `CapabilityDescriptions`, `Caption`, `CarrierSenseErrors`, `ConnectionName`, `ControllerID`, `CreationClassName`, `DDPPackageName`, `DDPPackageTrackId`, `DDPPackageVersion`, `DeferredTransmissions`, `Description`, `DeviceID`, `DeviceStatus`, `DeviceStatusString`, `DriverVersion`, `EEE`, `EEEString`, `EnabledCapabilities`, `ErrorCleared`, `ErrorDescription`, `ETrackID`, `ExcessiveCollisions`, `ExtendedStatus`, `FCSErrors`, `FrameTooLongs`, `FullDuplex`, `FWVersion`, `HardwareStatus`, `IdentifyingDescriptions`, `InstallDate`, `InternalMACReceiveErrors`, `InternalMACTransmitErrors`, `LastErrorCode`, `LateCollisions`, `Location`, `MaxDataSize`, `MaxQuiesceTime`, `MaxSpeed`, `MediaType`, `MediaTypeString`, `MiniPortInstance`, `MiniPortName`, `MultipleCollisionFrames`, `Name`, `NegotiatedLinkSpeed`, `NegotiatedLinkSpeedString`, `NegotiatedLinkWidth`, `NegotiatedLinkWidthString`, `NetlistVersion`, `NetworkAddresses`, `NVMVersion`, `OctetsReceived`, `OctetsTransmitted`, `OriginalDisplayName`, `OtherCapabilityDescriptions`, `OtherEnabledCapabilities`, `OtherEnabledCapabilityIDs`, `OtherIdentifyingInfo`, `OtherMediaType`, `OtherPhyDevice`, `OTPVersion`, `PartitionNumber`, `PartitionNumberString`, `PartNumber`, `PCIDeviceID`, `PermanentAddress`, `PHYDevice`, `PortNumber`, `PortNumberString`, `PowerManagementCapabilities`, `PowerManagementSupported`, `PowerOnHours`, `SanMacAddress`, `SingleCollisionFrames`, `SlotID`, `Speed`, `SpeedString`, `SQETestErrors`, `Status`, `StatusInfo`, `SymbolErrors`, `SystemCreationClassName`, `SystemName`, `Temperature`, `TemperatureString`, `TotalPacketsReceived`, `TotalPacketsTransmitted`, `TotalPowerOnHours`
- **Methods:**
  - `IdentifyAdapter`, `SetPowerMgmtCapabilities`, `SetSettings`, `GetSettingsForProfile`, etc.
- **Usage:**
  - Query adapter status, capabilities, and run management functions.

### 2. IANet_AdapterSettingInt / Enum / String / MultiString / Slider / MultiSelection
- **Description:** Represent various types of adapter settings (integer, enum, string, etc.).
- **Properties:**
  - `InstanceID`, `SettingName`, `Value`, `Writable`, `Description`, `DisplayName`, `Category`, `SubCategory`, `DefaultValue`, `MinValue`, `MaxValue`, `StepValue`, `Unit`, `PossibleValues[]`, `CurrentValue`, `PreviousValue`, `IsAdvanced`, `IsVisible`, `IsReadOnly`, `IsDeprecated`, `IsExperimental`, `IsProfileSpecific`, `ProfileName`, `ProfileID`, `SettingType`, `SettingGroup`, `SettingOrder`, `SettingScope`, `SettingSource`, `SettingStatus`, `SettingVersion`, `SettingOwner`, `SettingNotes`, `SettingHistory`, `SettingDependencies`, `SettingPrerequisites`, `SettingPostrequisites`, `SettingTriggers`, `SettingEvents`, `SettingLog`, `SettingLastModified`, `SettingLastApplied`, `SettingLastRead`, `SettingLastWritten`, `SettingLastReset`, `SettingLastRestored`, `SettingLastExported`, `SettingLastImported`, `SettingExportPath`, `SettingImportPath`, `SettingExportFormat`, `SettingImportFormat`, `SettingExportVersion`, `SettingImportVersion`, `SettingExportOwner`, `SettingImportOwner`, `SettingExportNotes`, `SettingImportNotes`, `SettingExportHistory`, `SettingImportHistory`, `SettingExportSchedule`, `SettingImportSchedule`, `SettingExportTriggers`, `SettingImportTriggers`, `SettingExportEvents`, `SettingImportEvents`
- **Methods:**
  - `GetValue()`, `SetValue(Value)`, `ResetValue()`, `ExportSetting(Path, Format)`, `ImportSetting(Path, Format)`, `GetSettingHistory()`, `GetSettingLog()`, `GetSettingEvents()`, `AcknowledgeSettingEvent()`, `AcknowledgeSettingWarning()`, `AcknowledgeSettingError()`, `AcknowledgeSettingInfo()`, `AcknowledgeSettingChange()`, `AcknowledgeSettingReset()`, `AcknowledgeSettingExport()`, `AcknowledgeSettingImport()`
- **Usage:**
  - Query and set advanced driver parameters (offloads, timestamping, PTP, etc.).

### 3. IANet_DiagTest, IANet_DiagResult, IANet_DiagSetting
- **Description:** Diagnostic test and result classes for running and retrieving adapter diagnostics.
- **Properties:**
  - `TestName`, `AdapterName`, `Result`, `Status`, `StartTime`, `EndTime`, `TestType`, `TestParameters`, `TestResultDetails`, `ErrorCode`, `ErrorDescription`, `Duration`, `Progress`, `IsRunning`, `IsCompleted`, `IsSuccessful`, `DiagnosticLog`, `RecommendedAction`, `LastRunTime`, `RunCount`, `PassCount`, `FailCount`, `WarningCount`, `InfoCount`, `TestCategory`, `TestSubCategory`, `TestSeverity`, `TestImpact`, `TestDependencies`, `TestPrerequisites`, `TestPostrequisites`, `TestOwner`, `TestVersion`, `TestDescription`, `TestNotes`, `TestHistory`, `TestSchedule`, `TestTriggers`, `TestEvents`, `TestResultsHistory`, `TestResultSummary`, `TestResultDetails`, `TestResultLog`, `TestResultTimestamp`, `TestResultStatus`, `TestResultCode`, `TestResultMessage`, `TestResultData`, `TestResultParameters`, `TestResultOwner`, `TestResultVersion`, `TestResultDescription`, `TestResultNotes`, `TestResultHistory`, `TestResultSchedule`, `TestResultTriggers`, `TestResultEvents`
- **Methods:**
  - `RunTest(Adapter, TestParameters, [ref]Result)`, `ClearResults()`, `DiscontinueTest()`, `ValidateTest()`, `GetTestStatus()`, `GetTestResult()`, `GetTestHistory()`, `GetTestLog()`, `GetTestSummary()`, `GetTestDetails()`, `GetTestParameters()`, `SetTestParameters()`, `ScheduleTest()`, `TriggerTest()`, `CancelTest()`, `PauseTest()`, `ResumeTest()`, `RestartTest()`, `ResetTest()`, `ExportTestResults()`, `ImportTestResults()`, `AcknowledgeTestResult()`, `AcknowledgeTestEvent()`, `AcknowledgeTestWarning()`, `AcknowledgeTestError()`, `AcknowledgeTestInfo()`, `AcknowledgeTestPass()`, `AcknowledgeTestFail()`, `AcknowledgeTestComplete()`, `AcknowledgeTestStart()`, `AcknowledgeTestStop()`, `AcknowledgeTestPause()`, `AcknowledgeTestResume()`, `AcknowledgeTestCancel()`, `AcknowledgeTestRestart()`, `AcknowledgeTestReset()`, `AcknowledgeTestExport()`, `AcknowledgeTestImport()`
- **Usage:**
  - Automate diagnostics, cable tests, and retrieve results programmatically.

### 4. IANet_NetService
- **Description:** Provides network service management for adapters.
- **Properties:**
  - `ServiceName`, `Status`, `AdapterName`, `ServiceType`, `ServiceStatus`, `ServiceDescription`, `ServiceStartType`, `ServiceDependencies`, `ServiceAccount`, `ServiceDisplayName`, `ServicePath`, `ServiceParameters`, `ServiceLogon`, `ServicePID`, `ServiceUptime`, `ServiceLastStartTime`, `ServiceLastStopTime`, `ServiceRestartCount`, `ServiceErrorControl`, `ServiceFailureActions`, `ServiceRecoveryOptions`, `ServiceTriggers`, `ServiceEvents`, `ServiceLog`, `ServiceHistory`, `ServiceOwner`, `ServiceVersion`, `ServiceNotes`, `ServiceSettings`, `ServiceProfiles`, `ServiceCapabilities`, `ServiceFeatures`, `ServiceSupportedProtocols`, `ServiceCurrentProtocol`, `ServiceSupportedProfiles`, `ServiceCurrentProfile`, `ServiceSupportedSettings`, `ServiceCurrentSettings`, `ServiceSupportedFeatures`, `ServiceCurrentFeatures`, `ServiceSupportedEvents`, `ServiceCurrentEvents`, `ServiceSupportedDiagnostics`, `ServiceCurrentDiagnostics`, `ServiceSupportedStatistics`, `ServiceCurrentStatistics`, `ServiceSupportedQoSProfiles`, `ServiceCurrentQoSProfile`, `ServiceSupportedVLANs`, `ServiceCurrentVLANs`, `ServiceSupportedLinkSpeeds`, `ServiceCurrentLinkSpeeds`, `ServiceSupportedDuplexModes`, `ServiceCurrentDuplexModes`, `ServiceSupportedMTU`, `ServiceCurrentMTUs`, `ServiceSupportedMACAddresses`, `ServiceCurrentMACAddresses`, `ServiceSupportedSRIOV`, `ServiceCurrentSRIOVs`, `ServiceSupportedRSS`, `ServiceCurrentRSSs`, `ServiceSupportedInterruptModeration`, `ServiceCurrentInterruptModerations`, `ServiceSupportedFlowControl`, `ServiceCurrentFlowControls`, `ServiceSupportedJumboPacket`, `ServiceCurrentJumboPackets`, `ServiceSupportedPTP`, `ServiceCurrentPTPs`, `ServiceSupportedTimestamping`, `ServiceCurrentTimestampings`, `ServiceSupportedOffloads`, `ServiceCurrentOffloads`, `ServiceSupportedPowerManagement`, `ServiceCurrentPowerManagements`, `ServiceSupportedWakeOnLAN`, `ServiceCurrentWakeOnLANs`
- **Methods:**
  - `Apply()`, `Rollback()`, `StartService()`, `StopService()`, `RestartService()`, `PauseService()`, `ResumeService()`, `EnableService()`, `DisableService()`, `QueryServiceStatus()`, `QueryServiceConfig()`, `SetServiceConfig()`, `GetServiceLog()`, `ClearServiceLog()`, `GetServiceHistory()`, `GetServiceEvents()`, `AcknowledgeServiceEvent()`, `AcknowledgeServiceWarning()`, `AcknowledgeServiceError()`, `AcknowledgeServiceInfo()`, `AcknowledgeServiceStart()`, `AcknowledgeServiceStop()`, `AcknowledgeServicePause()`, `AcknowledgeServiceResume()`, `AcknowledgeServiceRestart()`, `AcknowledgeServiceEnable()`, `AcknowledgeServiceDisable()`, `AcknowledgeServiceApply()`, `AcknowledgeServiceRollback()`
- **Usage:**
  - Apply or rollback configuration changes.

### 5. CIM_* Classes
- **Description:** Standard DMTF CIM classes for device, diagnostic, and configuration management. Used as base classes for Intel-specific extensions.

## Example WMI Queries

- **List all Intel adapters:**
  ```powershell
  Get-WmiObject -Namespace "root\IntelNCS2" -Class "IANet_PhysicalEthernetAdapter"
  ```
- **List all available settings for an adapter:**
  ```powershell
  Get-WmiObject -Namespace "root\IntelNCS2" -Class "IANet_AdapterSettingInt"
  Get-WmiObject -Namespace "root\IntelNCS2" -Class "IANet_AdapterSettingEnum"
  ```
- **Run a diagnostic test:**
  ```powershell
  $adapter = Get-WmiObject -Namespace "root\IntelNCS2" -Class "IANet_PhysicalEthernetAdapter" | Select-Object -First 1
  $diag = Get-WmiObject -Namespace "root\IntelNCS2" -Class "IANet_DiagTest" | Where-Object { $_.AdapterName -eq $adapter.Name }
  $diag.RunTest($adapter, $null, [ref]$null)
  ```

## Implementation Notes
- The WMI interface allows both querying and (where `Writable=true`) setting advanced driver features.
- Not all settings are writable; check the `Writable` property.
- Some features (e.g., hardware timestamping, PTP) may be exposed as settings or capabilities.
- Diagnostics and management functions can be automated via WMI.

## Next Steps
1. Enumerate all available classes and properties on a target system using PowerShell or WMI tools.
2. Identify settings relevant to PTP, timestamping, and advanced offloads.
3. Design a C++/C# DLL that wraps these WMI queries and exposes a clean API for the gPTP codebase.
4. Implement and test feature detection and configuration using this interface.

---

**This document serves as the foundation for implementing a robust Windows-specific driver interface for Intel network adapters.**
