/*******************************************************************************
 *
 * Copyright 2024 Portable Software Company
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
//using System.Security.Permissions;
using System.Security;
using System.Resources;

// General Information about an assembly is controlled through the following 
// set of attributes. Change these attribute values to modify the information
// associated with an assembly.
[assembly: AssemblyTitle("FS6 File Interface .NET")]
[assembly: AssemblyDescription("")]
[assembly: AssemblyConfiguration("")]
[assembly: AssemblyCompany("Portable Software Company")]
[assembly: AssemblyProduct("FS6 File Interface .NET")]
[assembly: AssemblyCopyright("Copyright (c) Portable Software Company 2023")]
[assembly: AssemblyTrademark("")]
[assembly: AssemblyCulture("")]
//[assembly: SecurityPermission(SecurityAction.RequestMinimum,
//  Flags = SecurityPermissionFlag.UnmanagedCode | SecurityPermissionFlag.ControlThread | SecurityPermissionFlag.ControlEvidence)]
//[assembly: FileIOPermission(SecurityAction.RequestMinimum, Unrestricted=true)]
//[assembly: UIPermission(SecurityAction.RequestMinimum, Window = UIPermissionWindow.AllWindows, Clipboard = UIPermissionClipboard.OwnClipboard)]
//[assembly: ReflectionPermission(SecurityAction.RequestMinimum,Unrestricted=true)]
//[assembly: KeyContainerPermission(SecurityAction.RequestMinimum,Unrestricted=true)]
[assembly: SecurityRules(SecurityRuleSet.Level2)]
[assembly: SecurityTransparent]

// Setting ComVisible to false makes the types in this assembly not visible 
// to COM components.  If you need to access a type in this assembly from 
// COM, set the ComVisible attribute to true on that type.
[assembly: ComVisible(false)]

// The following GUID is for the ID of the typelib if this project is exposed to COM
[assembly: Guid("757b1434-2d62-4e04-aee4-a59c8b7077cb")]

// Version information for an assembly consists of the following four values:
//
//      Major Version
//      Minor Version 
//      Build Number
//      Revision
//
[assembly: AssemblyVersion("6.0.2.0")]
[assembly: AssemblyFileVersion("6.0.2.0")]
[assembly: CLSCompliant(true)]
[assembly: NeutralResourcesLanguageAttribute("en-US")]
