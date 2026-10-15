# Wio Error and Status Code Reference

Generated from maintained implementation sources by `tools/Update-DiagnosticReference.ps1`.
This is the current development-source inventory, not a promise that every code exists in every installed release.

## Scope and interpretation

- Includes every literal WIR/LIR/WIRN/WCPP/WBC/WVM code found in compiler, app, runtime, VM, SDK, CLI, scripts and tools C++/Wio sources. Test expectations and historical documents are not definitions.
- Each entry gives source message templates and linked definition sites. `{dynamic context}` denotes runtime details; it is not literal output. One ID may have several context-dependent definitions.
- Dynamic-only messages are explicitly marked and given a reviewed category explanation. Follow the source expression and surrounding validation for their exact details. Reviewed overrides also distinguish conditional code/message branches.
- Verifier errors mean an IR/bytecode invariant was violated; valid source reaching them can indicate a compiler defect. Other codes describe ordinary runtime failures such as bounds, lifetime, cancellation or foreign-boundary errors.
- Native/module status enums, SDK exception categories and std error/domain enums are listed separately below. Pending/ready/success states are not errors.
- Legacy lexer/parser/analyzer/CLI messages without assigned stable IDs cannot have code definitions here. User/custom Result codes and native OS error numbers are open domains, not a finite Wio registry.
- This is a source inventory, not the future normative structured diagnostic registry, severity/fix-it catalog or `wio explain` implementation.

Regenerate: `pwsh -File tools/Update-DiagnosticReference.ps1`.
Check freshness: `pwsh -File tools/Update-DiagnosticReference.ps1 -Check`.

Distinct diagnostic IDs: **693**.

## Diagnostic families

| Family | IDs | Meaning |
| --- | ---: | --- |
| [WIR](#wir) | 221 | Typed WIR building/verification, specialization and canonical lowering. |
| [LIR](#lir) | 129 | Lowered WIR verification and canonical ownership/control-flow invariants. |
| [WIRN](#wirn) | 6 | Native ABI/thunk planning invariants. |
| [WCPP](#wcpp) | 25 | Lowered WIR C++ backend validation and emission. |
| [WBC](#wbc) | 94 | Bytecode compilation, format decoding and verification. |
| [WVM](#wvm) | 218 | VM loading/execution, intrinsics, native transport and lifetime failures. |

## WIR

### WIR1000

- Typed WIR module name cannot be empty.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:331](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L331).

### WIR1001

- Typed WIR type table contains an invalid type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:338](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L338).

### WIR1002

- Typed WIR type references an unknown type id.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:349](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L349).

### WIR1003

- Named Typed WIR types require a stable logical name.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:352](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L352).

### WIR1004

- Unary constructed Typed WIR type requires exactly one type argument.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:361](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L361).

### WIR1005

- Dictionary Typed WIR type requires key and value arguments.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:364](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L364).

### WIR1006

- Only array Typed WIR types may carry a static extent.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:366](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L366).

### WIR1007

- Only named Typed WIR types may carry a nominal kind.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:368](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L368).

### WIR1008

- Native POD representation requires a named component, enum, or flagset type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:376](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L376).

### WIR1009

- Only named Typed WIR types may carry layout or lifecycle metadata.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:387](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L387).

### WIR1010

- Typed WIR field layouts require unique names and valid field types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:411](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L411).

### WIR1011

- Typed WIR nominal base layouts must reference named types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:417](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L417).

### WIR1012

- Typed WIR method layouts require names, functions, valid signatures, and unique slots.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:428](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L428).

### WIR1013

- Typed WIR method layout references an unknown or non-method function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:776](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L776).

### WIR1014

- Generic parameter Typed WIR types require a stable source name.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:356](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L356).

### WIR1015

- Only named Typed WIR types may carry a nominal value model.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:370](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L370).

### WIR1016

- Typed WIR ownership and cleanup metadata are inconsistent.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:405](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L405).

### WIR1017

- Native POD types require canonical C++ type binding metadata.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:380](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L380).

### WIR1018

- Only Native POD types may carry native type binding metadata.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:382](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L382).

### WIR1019

- Typed WIR enum/flagset layout requires an integer underlying type.
- Only enum/flagset Typed WIR types may carry case layout metadata.
- Typed WIR enum/flagset cases require unique non-empty names.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:393](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L393), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:395](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L395), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:399](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L399).

### WIR1020

- Typed WIR global id must be valid and unique.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:438](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L438).

### WIR1021

- Typed WIR global requires a name, type, and consistent mutability.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:440](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L440).

### WIR1022

- Typed WIR global initializer must be a known zero-argument function returning the global type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:784](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L784).

### WIR1023

- Symbolic array extent must identify a const generic parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:344](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L344).

### WIR1100

- Typed WIR function has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:448](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L448).

### WIR1101

- Typed WIR function id is duplicated.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:452](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L452).

### WIR1102

- Typed WIR function name cannot be empty.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:454](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L454).

### WIR1103

- Typed WIR function return type is invalid.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:456](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L456).

### WIR1104

- External Typed WIR functions cannot contain blocks.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:794](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L794).

### WIR1105

- Defined Typed WIR functions require at least one block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:800](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L800).

### WIR1106

- Typed WIR methods require a named owner and leading self reference parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:535](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L535).

### WIR1107

- Non-method Typed WIR functions cannot carry an owner type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:541](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L541).

### WIR1108

- Typed WIR function requires a callable type ending in its return type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:474](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L474).

### WIR1109

- Typed WIR generic parameter metadata must reference generic parameter types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:504](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L504).

### WIR1110

- Typed WIR closure capture layout and hidden parameter count are inconsistent.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:511](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L511).

### WIR1111

- Typed WIR closure capture layout does not match its hidden parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:524](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L524).

### WIR1112

- Typed WIR visible parameters must match the callable type after hidden receiver/capture parameters.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:490](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L490).

### WIR1113

- Native Typed WIR function binding has an invalid identity, ABI signature, or thunk contract.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:569](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L569).

### WIR1114

- Async Typed WIR function requires coroutine&lt;T&gt; return and matching coroutine layout.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:463](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L463).

### WIR1115

- Non-async Typed WIR function cannot carry coroutine layout.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:468](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L468).

### WIR1200

- Typed WIR value has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:818](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L818).

### WIR1201

- Typed WIR value has an invalid type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:822](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L822).

### WIR1202

- Typed WIR value id is defined more than once.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:833](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L833).

### WIR1204

- Typed WIR parameter ownership does not match its type contract.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:824](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L824).

### WIR1205

- Borrowed Typed WIR values require lifetime metadata, and owned values cannot carry it.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:828](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L828).

### WIR1300

- Typed WIR block has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:845](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L845).

### WIR1301

- Typed WIR block id is duplicated.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:847](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L847).

### WIR1302

- Typed WIR block requires a terminator.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:960](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L960).

### WIR1303

Definition: Typed WIR terminator must be the final instruction in its block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:970](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L970).

### WIR1304

Definition: Typed WIR block must end with a terminator.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:970](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L970).

### WIR1400

- Typed WIR instruction references an unknown value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:980](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L980).

### WIR1401

- Value-producing Typed WIR instruction requires a typed result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1064](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1064).

### WIR1402

- Typed WIR terminator cannot produce a value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1067](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1067).

### WIR1403

- Typed WIR constant literal does not match its result type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1104](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1104).

### WIR1404

- Typed WIR unary instruction requires one operand matching its result type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1111](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1111).

### WIR1405

- Typed WIR binary operands must have the same type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1120](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1120).

### WIR1406

- Typed WIR comparison result must be bool.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1126](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1126).

### WIR1407

- Typed WIR binary result must match its operand type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1131](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1131).

### WIR1408

- Typed WIR select requires bool condition and matching result arms.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1794](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1794).

### WIR1409

- Typed WIR call references an unknown function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2048](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2048).

### WIR1410

- Typed WIR call argument count does not match its callee.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2067](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2067).

### WIR1411

- Typed WIR call argument type does not match its parameter (operand={dynamic context}, signature={dynamic context}, parameter={dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2089](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2089).

### WIR1412

- Typed WIR call result does not match its callee return type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2106](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2106).

### WIR1413

- Typed WIR return value does not match its function return type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2155](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2155).

### WIR1414

- Typed WIR branch requires exactly one target.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2162](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2162).

### WIR1415

- Typed WIR conditional branch requires a bool condition and two targets.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2190](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2190).

### WIR1416

- Typed WIR instruction references an unknown block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2211](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2211).

### WIR1417

- Typed WIR branch argument count does not match its target block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2169](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2169).

### WIR1418

- Typed WIR branch argument type does not match its target block parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2178](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2178).

### WIR1419

- Typed WIR conditional branch targets cannot require block arguments.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2200](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2200).

### WIR1420

- Typed WIR value is used before its definition in the same block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:993](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L993).

### WIR1421

- Typed WIR value definition does not dominate its use.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1002](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1002).

### WIR1422

- Typed WIR numeric conversion requires one numeric operand and a numeric result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1157](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1157).

### WIR1423

- Typed WIR variant test requires one named value, a variant selector, and a bool result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1170](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1170).

### WIR1424

- Typed WIR variant payload requires one named value and a variant selector.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1191](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1191).

### WIR1425

- Typed WIR array length requires one array value and a usize result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1205](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1205).

### WIR1426

- Typed WIR array element projection must return its array element type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1217](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1217).

### WIR1427

- Typed WIR array creation requires element operands matching its array type and extent.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1235](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1235).

### WIR1428

- Typed WIR array get requires an array, an integer index, and its element result type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1253](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1253).

### WIR1429

- Typed WIR local place requires a named reference result and no operands.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1497](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1497).

### WIR1430

Definition: Typed WIR place initialization requires a reference place and a matching value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1541](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1541).

### WIR1431

Definition: Typed WIR store requires a mutable reference place and a matching value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1541](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1541).

### WIR1432

- Typed WIR load requires a reference place and its referred result type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1573](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1573).

### WIR1433

- Typed WIR field place requires a named base, field selector, and non-strengthening reference result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1606](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1606).

### WIR1434

- Typed WIR array place requires an array reference, integer index, and non-strengthening element reference.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1654](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1654).

### WIR1435

- Typed WIR borrow cannot change the referred type or strengthen mutability.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1707](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1707).

### WIR1436

- Typed WIR place initialization must target an uninitialized local place exactly once.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1558](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1558).

### WIR1437

- Typed WIR field place must match the nominal field layout and field mutability (owner={dynamic context}, selector='{dynamic context}', expected={dynamic context}, actual={dynamic context}, field-mutable={dynamic context}true{dynamic context}false{dynamic context}, place-mutable={dynamic context}true{dynamic context}false{dynamic context}, constructor={dynamic context}true{dynamic context}false{dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1621](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1621).

### WIR1438

- Typed WIR construction requires a matching constructible component/object result and typed constructor signature.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1729](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1729).

### WIR1439

- Typed WIR drop requires a place containing a cleanup-bearing value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1783](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1783).

### WIR1440

- Typed WIR method dispatch must match its owner slot, receiver, signature, and callee (selector='{dynamic context}', slot={dynamic context}, owner={dynamic context}, receiver={dynamic context}, callee=#{dynamic context}, operands={dynamic context}, signatures={dynamic context}, dispatch-valid={dynamic context}true{dynamic context}false{dynamic context}, callee-found={dynamic context}true{dynamic context}false{dynamic context}, method-found={dynamic context}true{dynamic context}false{dynamic context}, result={dynamic context}, method-result={dynamic context}, callee-result={dynamic context}, callee-generic={dynamic context}true{dynamic context}false{dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1970](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1970).

### WIR1441

- Typed WIR object cast requires compatible object/interface source and target types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2011](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2011).

### WIR1442

- Typed WIR type test requires an object/interface value, target type, and bool result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2023](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2023).

### WIR1443

- Typed WIR identity equality requires two object/interface values and a bool result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2038](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2038).

### WIR1444

- Typed WIR callable generic arguments must reference valid types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1072](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1072).

### WIR1445

- Typed WIR function reference must pin a known callable declaration and callable type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1809](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1809).

### WIR1446

- Typed WIR closure creation must match its closure body capture layout exactly.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1838](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1838).

### WIR1447

- Typed WIR indirect call must match its function value signature.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1869](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1869).

### WIR1448

- Typed WIR extension call must pin its implementation, receiver, signature, and specialization.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1901](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1901).

### WIR1455

- Typed WIR Option/Result variant test has an invalid variant selector.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1180](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1180).

### WIR1456

- Typed WIR dictionary creation requires alternating key/value operands matching its concrete dictionary type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1273](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1273).

### WIR1457

- Typed WIR dictionary get requires matching dictionary, key, and mapped result types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1289](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1289).

### WIR1458

- Typed WIR interpolation requires string/text identity, one more segment than values, and concrete value signatures.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1312](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1312).

### WIR1459

- Typed WIR enum constant requires a named enum/flagset result and stable member selector.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1327](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1327).

### WIR1460

Definition: Intrinsic call metadata is incomplete or inconsistent: family, selector, receiver, target type and concrete operand signature must agree. The diagnostic includes the actual types/counts.

Detailed message: dynamically supplied by the reporting path.
Message expression: `std::move(detail)`.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1366](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1366).

### WIR1461

- Typed WIR any box requires one operand and its materialized payload type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1377](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1377).

### WIR1462

- Typed WIR any test/cast requires one any operand and a valid concrete target type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1393](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1393).

### WIR1463

- Typed WIR nullable wrap requires one value matching the nullable payload type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1406](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1406).

### WIR1464

- Typed WIR dictionary place requires a dictionary reference, matching key, and mapped-value reference.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1677](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1677).

### WIR1465

- Typed WIR load ownership must be borrowed for cleanup-bearing values in function '{dynamic context}' (expected={dynamic context}, actual={dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1582](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1582).

### WIR1466

- Typed WIR copy must create one owned claim for a cleanup-bearing value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1744](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1744).

### WIR1467

- Typed WIR move must transfer a cleanup-bearing place into one owned value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1758](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1758).

### WIR1468

- Typed WIR release requires one cleanup-bearing owned value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1769](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1769).

### WIR1469

- Cleanup-bearing place is initialized while already live.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2259](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2259).

### WIR1470

- Typed WIR native calls and ordinary calls must match the callee ABI binding.
- Cleanup-bearing place is moved or dropped more than once.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2057](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2057), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2267](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2267).

### WIR1471

- Replace requires an initialized cleanup-bearing place.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2274](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2274).

### WIR1472

- Function exit leaves cleanup-bearing local places live.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2278](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2278).

### WIR1473

- Typed WIR async metadata is attached to an unsupported instruction.
- Control-flow merge disagrees on live cleanup-bearing places.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1080](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1080), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2292](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2292).

### WIR1474

- Typed WIR await requires an async function, one coroutine&lt;T&gt;, and its exact payload result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2128](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2128).

### WIR1475

- Typed WIR executor switch requires an async function and an explicit destination executor.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2140](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2140).

### WIR1477

- Typed WIR iterator creation requires a canonical range/array/dictionary source.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1440](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1440).

### WIR1478

- Typed WIR iterator operation requires one iterator and a canonical result shape.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1458](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1458).

### WIR1479

- Typed WIR Result operation requires canonical Result&lt;T&gt; source and payload/propagation types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1486](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1486).

### WIR1480

- Typed WIR global-place must match a declared global and its mutability.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1509](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1509).

### WIR1481

- Typed WIR pack expansion metadata must align with symbolic pack operands.
- Typed WIR nullable unwrap requires one nullable value and its payload result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1021](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1021), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1421](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1421).

### WIR1482

- Typed WIR range containment requires three equal numeric operands and a bool result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1143](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1143).

### WIR1490

- Generic constant must identify a typed const parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1098](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1098).

### WIR1491

- Default value requires a value type and no operands.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1089](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1089).

### WIR1500

- Typed WIR module contract requires a canonical stable identity.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:587](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L587).

### WIR1501

- Typed WIR SDK call table version, identity, and export cardinality must be canonical.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:593](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L593).

### WIR1502

- Typed WIR module imports require unique stable identities and logical names.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:598](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L598).

### WIR1503

- Typed WIR export descriptor has an invalid stable identity, slot, signature, or target.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:636](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L636).

### WIR1504

- Typed WIR reflection descriptors require unique identities and matching nominal types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:647](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L647).

### WIR1505

- Typed WIR module lifecycle must reference known functions and pair save/restore state hooks.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:767](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L767).

### WIR1506

- Typed WIR reflected fields require unique identities, names, and valid types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:653](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L653).

### WIR1507

- Typed WIR reflected methods require unique identities and valid callable signatures.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:657](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L657).

### WIR1508

- Typed WIR attribute applications require unique identities and valid targets.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:674](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L674).

### WIR1509

- Typed WIR attribute processors require a stable phase, identity, executable hook, and value type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:688](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L688).

### WIR1510

- Typed WIR reflected type references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:698](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L698).

### WIR1511

- Typed WIR reflected field references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:701](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L701).

### WIR1512

- Typed WIR reflected method references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:704](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L704).

### WIR1513

- Typed WIR systems require unique component types and valid lifecycle functions.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:719](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L719).

### WIR1514

- Typed WIR application requires a component type, entry/lifecycle functions, and declared systems.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:733](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L733).

### WIR1515

- Typed WIR application stages require canonical order, dependencies, and frequency.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:744](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L744).

### WIR1516

- Typed WIR application runs require resolved targets, functions, and resources.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:755](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L755).

### WIR1517

- Typed WIR reflected enum/flagset cases require unique identities and names.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:661](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L661).

### WIR1518

- Typed WIR reflected enum/flagset case references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:707](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L707).

### WIR2000

- Cannot build Typed WIR from a null program.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:40](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L40).

### WIR2001

- Semantic type is missing while building Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2201](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2201).

### WIR2002

- Unsupported semantic primitive type '{dynamic context}' while mapping {dynamic context}an unknown source{dynamic context}.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2231](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2231).

### WIR2003

- Semantic type '{dynamic context}' has no backend-neutral WIR representation.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2606](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2606).

### WIR2100

- Function declaration is missing its resolved semantic function type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3064](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3064).

### WIR2101

- Function parameter is missing its resolved semantic symbol or type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3138](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3138).

### WIR2102

- Non-void function does not end with a return.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3253](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3253).

### WIR2103

- Async function must have a resolved coroutine&lt;T&gt; return type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3087](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3087).

### WIR2201

- Statement kind '{dynamic context}' has no Typed WIR lowering.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3459](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3459).

### WIR2202

- Local variable declaration is missing its semantic symbol.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3339](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3339).

### WIR2203

- Default initialization for type '{dynamic context}' is not supported by Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5923](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5923).

### WIR2204

- Loop-carried local value is unavailable on a control-flow edge.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6808](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6808).

### WIR2205

Definition: Break statement has no active Typed WIR loop target.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6821](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6821).

### WIR2206

Definition: Continue statement has no active Typed WIR loop target.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6821](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6821).

### WIR2210

- For-in requires a range, array, or dictionary iterator source.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:7010](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L7010).

### WIR2211

- For-in binding is missing its semantic symbol.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:7100](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L7100).

### WIR2300

- Integer literal has no representable WIR integer type.
- Integer literal cannot be represented by the initial Typed WIR literal model.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3789](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3789), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3796](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3796).

### WIR2301

- Identifier is not a value available in the current Typed WIR function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4182](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4182).

### WIR2302

- Unary expression could not be lowered to Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4511](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4511).

### WIR2303

- Binary expression could not be lowered to Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4653](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4653).

### WIR2304

- Call target has no indexed WIR function identity.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5137](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5137).

### WIR2305

- Expression kind '{dynamic context}' has no Typed WIR lowering.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5242](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5242).

### WIR2306

- Await requires a resolved coroutine&lt;T&gt; operand.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4423](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4423).

### WIR2308

- Compound assignment operator is not supported by Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4381](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4381).

### WIR2309

- Float literal has no representable WIR floating-point type.
- Float literal cannot be represented by the Typed WIR literal model.
- Assignment target did not lower to a reference place.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3810](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3810), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3816](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3816), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4368](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4368).

### WIR2310

- Char literal must contain exactly one byte-sized character.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3871](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3871).

### WIR2311

- Byte literal is outside the u8 range.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3884](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3884).

### WIR2313

- Typed WIR fit lowering requires numeric or object/interface operands.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4101](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4101).

### WIR2314

- Duration literal cannot be represented by Typed WIR.
- Expression requires a conversion that is not allowed by Wio numeric promotion.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3920](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3920), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5854](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5854).

### WIR2315

- Match expression requires at least one case.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6091](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6091).

### WIR2316

- Typed WIR does not recognize destructuring variant '{dynamic context}'.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5979](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5979).

### WIR2317

- Value-producing Typed WIR match cases require expression bodies.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6105](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6105).

### WIR2319

- Match binding is missing its semantic symbol.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6003](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6003).

### WIR2320

- Array literal requires a resolved WIR array element type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3930](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3930).

### WIR2322

- Typed WIR index access requires an array, dictionary, string, or text value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4330](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4330).

### WIR2323

- Addressable expression did not produce a reference place.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:932](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L932).

### WIR2324

- A mutable place was requested from a read-only view.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:937](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L937).

### WIR2325

- Initial Typed WIR member reads currently require a resolved data field.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4280](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4280).

### WIR2326

- Addressable identifier is missing its semantic symbol.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5274](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5274).

### WIR2327

- Addressable identifier is not available in the current Typed WIR function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5309](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5309).

### WIR2328

- Only array and dictionary index expressions are addressable WIR places.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5368](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5368).

### WIR2329

- Addressable member must resolve to a data field.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5392](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5392).

### WIR2330

- Expression kind '{dynamic context}' is not an addressable Typed WIR place.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5530](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5530).

### WIR2331

- 'self' is unavailable outside a receiver-aware WIR function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4068](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4068), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5256](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5256).

### WIR2332

- 'super' is unavailable outside a receiver-aware WIR function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4075](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4075).

### WIR2333

- Object/interface type test is missing a representable target type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4546](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4546).

### WIR2340

- Lambda parameter is missing its resolved callable type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2867](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2867).

### WIR2341

- Non-void lambda does not end with a return.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2951](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2951).

### WIR2342

- Lambda capture '{dynamic context}' is unavailable in the enclosing callable.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4014](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4014).

### WIR2343

- Lambda requested a self capture outside a receiver-aware callable.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4040](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4040).

### WIR2344

- Indirect call requires a resolved function value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5103](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5103).

### WIR2345

- Dictionary literal requires resolved key and value types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3960](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3960).

### WIR2350

- Result unwrap/propagation requires a canonical std::Result&lt;T&gt; value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3643](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3643).

### WIR2351

- The backend-neutral 'in' operation currently requires a range.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4563](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4563).

### WIR2352

- Overloaded operator is missing its resolved callable identity.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3571](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3571).

### WIR2353

- Member operator is missing its nominal owner slot.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3587](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3587).

### WIR2357

- Increment/decrement operand did not lower to a mutable place.
- Derived extension is missing its processor method layout.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4458](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4458), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4867](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4867).

### WIR2358

- Increment/decrement requires a representable numeric WIR type.
- Extension call is missing a materializable default argument.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4469](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4469), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4964](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4964).

### WIR2359

- Constructor call is missing a materializable default argument.
- Derived extension call is missing a materializable default argument.
- Method call is missing a materializable default argument.
- Implicit self method call is missing a materializable default argument.
- Function call is missing a materializable default argument.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4764](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4764), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4911](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4911), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5057](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5057), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5182](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5182), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5219](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5219).

### WIR2360

- Implicit field '{dynamic context}' is not mutable.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:871](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L871).

### WIR2361

- Pack array materialization requires a runtime value pack.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4236](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4236).

### WIR2362

- Implicit self method call is missing its canonical owner slot.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5155](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5155).

### WIR2370

- Enum/flagset member requires a canonical integer constant value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:1369](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L1369).

### WIR2401

- NativeCallback lifetime must be 'call' or 'retained'.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:676](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L676).

### WIR2402

- NativeCallback thread must be 'caller' or 'any'.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:680](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L680).

### WIR2403

- NativeConsume requires an owning value, not ref/view, opaque, callback or task storage.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:659](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L659).

### WIR2404

- NativeConsume is only supported on native function parameters.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3196](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3196).

### WIR3001

- Typed select instruction has an invalid shape.

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:532](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L532).

### WIR3002

- Typed branch requires exactly one target before lowering.

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:575](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L575).

### WIR3003

- Typed conditional branch has an invalid shape before lowering.

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:591](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L591).

### WIR3004

- Suspension instruction appears outside an async function.

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:468](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L468).

### WIR3100

Definition: Generic function/type materialization failed. The detailed message identifies the concrete substitution, pack/constant/layout/capture or specialization invariant that failed; there is no single fixed payload.

Detailed message: dynamically supplied by the reporting path.
Message expression: `std::move(message)`.

Definitions: [compiler/src/wio_compiler/wir/generic_specializer.cpp:575](../compiler/src/wio_compiler/wir/generic_specializer.cpp#L575).

### WIR3101

- Generic materialization of '{dynamic context}' exceeded its body limit ({dynamic context}); possible expanding polymorphic recursion.

Definitions: [compiler/src/wio_compiler/wir/generic_specializer.cpp:1585](../compiler/src/wio_compiler/wir/generic_specializer.cpp#L1585).

### WIR3200

- lower-object-hierarchy

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:674](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L674).

## LIR

### LIR1000

- Lowered WIR module name cannot be empty.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:223](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L223).

### LIR1001

- Lowered WIR type table contains an invalid type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:234](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L234).

### LIR1002

- Lowered WIR type references an unknown type id.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:238](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L238).

### LIR1003

- Only named Lowered WIR types may carry a nominal kind.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:241](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L241).

### LIR1004

- Native POD representation requires a named component, enum, or flagset type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:249](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L249).

### LIR1005

- Only named Lowered WIR types may carry layout or lifecycle metadata.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:260](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L260).

### LIR1006

- Lowered WIR field layouts require unique names and valid field types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:277](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L277).

### LIR1007

- Lowered WIR nominal base layouts must reference named types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:283](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L283).

### LIR1008

- Lowered WIR method layouts require valid signatures and unique slots.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:293](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L293).

### LIR1009

- Only named Lowered WIR types may carry a nominal value model.
- Lowered WIR method layout references an unknown or non-method function.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:243](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L243), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:610](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L610).

### LIR1010

- Native POD types require canonical C++ type binding metadata.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:253](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L253).

### LIR1011

- Only Native POD types may carry native type binding metadata.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:255](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L255).

### LIR1012

- Lowered WIR enum/flagset layout requires an integer underlying type.
- Only enum/flagset Lowered WIR types may carry case layout metadata.
- Lowered WIR enum/flagset cases require unique non-empty names.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:266](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L266), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:268](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L268), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:272](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L272).

### LIR1018

- Symbolic array extent must identify a const generic parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:231](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L231).

### LIR1020

- Lowered WIR global id must be valid and unique.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:301](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L301).

### LIR1021

- Lowered WIR global requires a name, type, and consistent mutability.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:303](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L303).

### LIR1022

- Lowered WIR global initializer must be a known zero-argument function returning the global type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:619](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L619).

### LIR1100

- Lowered WIR function has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:311](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L311).

### LIR1101

- Lowered WIR function id is duplicated.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:313](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L313).

### LIR1102

- Lowered WIR function name cannot be empty.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:315](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L315).

### LIR1103

- Lowered WIR function return type is invalid.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:317](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L317).

### LIR1104

- External Lowered WIR functions cannot contain blocks.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:651](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L651).

### LIR1105

- Defined Lowered WIR functions require at least one block.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:657](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L657).

### LIR1106

- Lowered WIR methods require a named owner and leading self parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:364](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L364).

### LIR1110

- Lowered WIR function requires a callable type ending in its return type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:332](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L332).

### LIR1111

- Lowered WIR closure capture metadata is inconsistent.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:355](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L355).

### LIR1112

- Lowered WIR visible parameters must match the callable type after hidden receiver/capture parameters.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:347](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L347).

### LIR1113

- Native Lowered WIR function binding has an invalid identity, ABI signature, or thunk contract.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:385](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L385).

### LIR1114

- Async Lowered WIR function requires coroutine&lt;T&gt; return and matching coroutine layout.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:323](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L323).

### LIR1115

- Non-async Lowered WIR function cannot carry coroutine layout.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:327](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L327).

### LIR1200

- Lowered WIR value has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:636](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L636).

### LIR1201

- Lowered WIR value has an invalid type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:640](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L640).

### LIR1202

- Lowered WIR value id is defined more than once.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:642](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L642).

### LIR1300

- Lowered WIR block has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:666](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L666).

### LIR1301

- Lowered WIR block id is duplicated.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:668](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L668).

### LIR1302

- Lowered WIR block requires a terminator.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:744](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L744).

### LIR1303

Definition: Lowered WIR terminator must be the final instruction in its block.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:754](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L754).

### LIR1304

Definition: Lowered WIR block must end with a terminator.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:754](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L754).

### LIR1400

- Lowered WIR instruction references an unknown value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:763](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L763).

### LIR1401

- Value-producing Lowered WIR instruction requires a typed result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:814](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L814).

### LIR1402

- Lowered WIR terminator cannot produce a value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:817](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L817).

### LIR1403

- Lowered WIR unary operand must match its result type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:937](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L937).

### LIR1404

- Lowered WIR binary operands must have the same type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:944](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L944).

### LIR1405

- Lowered WIR comparison result must be bool.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:948](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L948).

### LIR1406

- Lowered WIR binary result must match its operand type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:952](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L952).

### LIR1407

- Lowered WIR call references an unknown function.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1776](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1776).

### LIR1408

- Lowered WIR call argument count does not match its callee.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1792](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1792).

### LIR1409

- Lowered WIR call argument type does not match its parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1816](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1816).

### LIR1410

- Lowered WIR call result does not match its callee.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1828](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1828).

### LIR1411

- Lowered WIR return value does not match its function.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1924](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1924).

### LIR1412

- Lowered WIR jump shape is invalid.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1935](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1935).

### LIR1413

- Lowered WIR conditional jump condition must be bool.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1941](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1941).

### LIR1414

- Lowered WIR jump references an unknown block.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1952](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1952).

### LIR1415

- Lowered WIR jump argument count does not match target block parameters.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1959](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1959).

### LIR1416

- Lowered WIR jump argument type does not match target block parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1967](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1967).

### LIR1417

- Lowered WIR numeric conversion requires one numeric operand and a numeric result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:977](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L977).

### LIR1418

- Lowered WIR variant test requires one named value, a variant selector, and a bool result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:990](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L990).

### LIR1419

- Lowered WIR variant payload requires one named value and a variant selector.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1003](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1003).

### LIR1420

- Lowered WIR array length requires one array value and a usize result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1017](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1017).

### LIR1421

- Lowered WIR array element projection must return its array element type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1029](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1029).

### LIR1422

- Lowered WIR array creation requires element operands matching its array type and extent.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1048](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1048).

### LIR1423

- Lowered WIR array get requires an array, an integer index, and its element result type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1065](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1065).

### LIR1424

- Lowered WIR local place requires a named reference result and no operands.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1297](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1297).

### LIR1425

Definition: Lowered WIR place initialization requires a reference place and a matching value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1324](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1324).

### LIR1426

Definition: Lowered WIR store requires a mutable reference place and a matching value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1324](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1324).

### LIR1427

- Lowered WIR load requires a reference place and its referred result type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1353](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1353).

### LIR1428

- Lowered WIR field place requires a named base, field selector, and non-strengthening reference result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1373](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1373).

### LIR1429

- Lowered WIR array place requires an array reference, integer index, and non-strengthening element reference.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1423](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1423).

### LIR1430

- Lowered WIR borrow cannot change the referred type or strengthen mutability.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1476](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1476).

### LIR1431

- Lowered WIR place initialization must target an uninitialized local place exactly once.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1338](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1338).

### LIR1432

- Lowered WIR field place must match the nominal field layout and field mutability.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1398](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1398).

### LIR1433

- Lowered WIR construction requires a matching constructible component/object result and typed constructor signature.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1498](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1498).

### LIR1434

- Lowered WIR release/drop-place must match the stored value cleanup protocol.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1561](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1561).

### LIR1435

- Lowered WIR method dispatch must match its owner slot, receiver, signature, and callee.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1722](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1722).

### LIR1436

- Lowered WIR object cast requires compatible object/interface source and target types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1744](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1744).

### LIR1437

- Lowered WIR dictionary creation requires alternating key/value operands matching its concrete type.
- Lowered WIR type test requires an object/interface value, target type, and bool result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1086](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1086), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1754](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1754).

### LIR1438

- Lowered WIR dictionary get requires matching dictionary, key, and mapped result types.
- Lowered WIR identity equality requires object/interface operands and bool result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1101](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1101), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1767](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1767).

### LIR1439

- Lowered WIR interpolation requires string/text identity, segments, and concrete signatures.
- Lowered WIR function reference must pin a known callable declaration.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1125](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1125), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1573](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1573).

### LIR1440

- Lowered WIR enum constant requires a named enum/flagset result and stable selector.
- Lowered WIR closure creation must match its capture layout.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1141](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1141), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1599](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1599).

### LIR1441

- Lowered WIR intrinsic call requires a family, selector, receiver, target, and concrete signature.
- Lowered WIR indirect call must match its function value signature.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1163](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1163), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1627](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1627).

### LIR1442

- Lowered WIR any box requires one operand and its materialized payload type.
- Lowered WIR extension call must pin its implementation and concrete signature.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1177](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1177), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1659](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1659).

### LIR1443

- Lowered WIR any test/cast requires one any operand and a concrete target.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1193](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1193).

### LIR1444

- Lowered WIR nullable wrap requires one matching payload value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1206](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1206).

### LIR1445

- Lowered WIR iterator creation requires canonical source metadata.
- Lowered WIR dictionary place requires a dictionary reference, matching key, and mapped reference.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1241](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1241), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1446](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1446).

### LIR1446

- Lowered WIR iterator operation requires one iterator and a canonical result.
- Lowered WIR retain/copy-value must match the type cleanup protocol.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1258](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1258), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1517](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1517).

### LIR1447

- Lowered WIR move-value must transfer one cleanup-bearing place.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1531](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1531).

### LIR1448

- Lowered WIR release/drop-value must match the value cleanup protocol.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1544](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1544).

### LIR1450

- Lowered native-invoke and ordinary call must match the callee ABI binding.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1783](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1783).

### LIR1451

- Coroutine frame slots require unique dense ids, values, and matching ownership metadata.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:710](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L710).

### LIR1452

- Coroutine states require dense ids and valid suspend/resume/value references.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:724](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L724).

### LIR1453

- Lowered WIR async metadata is attached to an unsupported instruction.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:826](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L826).

### LIR1454

- Cancellation check must immediately precede its coroutine suspension point.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1840](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1840).

### LIR1455

- Coroutine suspension must match one canonical state and resume target.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1879](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1879).

### LIR1456

- Coroutine resume must materialize the payload declared by its canonical state.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1893](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1893).

### LIR1457

- Coroutine completion value must match the async payload type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1908](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1908).

### LIR1458

- Async Lowered WIR must use coroutine-complete instead of return.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1915](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1915).

### LIR1466

- Lowered WIR nullable unwrap requires one nullable value and its payload result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1220](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1220).

### LIR1479

- Lowered WIR Result operation requires canonical Result&lt;T&gt; source and payload/propagation types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1286](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1286).

### LIR1480

- Lowered WIR global-place must match a declared global and its mutability.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1309](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1309).

### LIR1481

- Lowered WIR pack expansion metadata must align with symbolic pack operands.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:781](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L781).

### LIR1482

- Lowered WIR range containment requires three equal numeric operands and a bool result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:962](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L962).

### LIR1483

- Lowered WIR storage and escape metadata is incompatible with {dynamic context} (storage={dynamic context}, escape={dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:853](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L853).

### LIR1484

- Lowered WIR bounds-check metadata must appear on every array access and nowhere else.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:866](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L866).

### LIR1485

- Lowered WIR eliminated bounds check requires a reproducible static or construction proof.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:910](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L910).

### LIR1490

- Generic constant must identify a typed const parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:922](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L922).

### LIR1491

- Default value requires a value type and no operands.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:930](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L930).

### LIR1500

- Lowered WIR module contract requires a canonical stable identity.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:440](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L440).

### LIR1501

- Lowered WIR SDK call table version, identity, and export cardinality must be canonical.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:446](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L446).

### LIR1502

- Lowered WIR export descriptor has an invalid stable identity, slot, or target.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:474](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L474).

### LIR1503

- Lowered WIR module lifecycle must reference known functions and pair save/restore state hooks.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:482](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L482).

### LIR1504

- Lowered WIR module imports require unique stable identities and logical names.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:451](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L451).

### LIR1505

- Lowered WIR reflection descriptors require unique identities and matching nominal types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:491](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L491).

### LIR1506

- Lowered WIR reflected fields require unique identities, names, and valid types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:497](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L497).

### LIR1507

- Lowered WIR reflected methods require unique identities and valid callable signatures.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:502](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L502).

### LIR1508

- Lowered WIR attribute applications require unique identities and valid targets.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:518](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L518).

### LIR1509

- Lowered WIR attribute processors require a stable phase, identity, executable hook, and value type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:532](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L532).

### LIR1510

- Lowered WIR reflected type references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:542](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L542).

### LIR1511

- Lowered WIR reflected field references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:545](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L545).

### LIR1512

- Lowered WIR reflected method references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:548](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L548).

### LIR1513

- Lowered WIR systems require unique component types and valid lifecycle functions.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:562](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L562).

### LIR1514

- Lowered WIR application requires a component type, entry/lifecycle functions, and declared systems.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:576](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L576).

### LIR1515

- Lowered WIR application stages require canonical order, dependencies, and frequency.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:587](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L587).

### LIR1516

- Lowered WIR application runs require resolved targets, functions, and resources.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:598](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L598).

### LIR1517

- Lowered WIR reflected enum/flagset cases require unique identities and names.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:506](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L506).

### LIR1518

- Lowered WIR reflected enum/flagset case references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:551](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L551).

### LIR1530

- Object hierarchy cast/dispatch/lifecycle table does not match canonical contracts.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:407](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L407), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:416](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L416).

### LIR1531

- Object dispatch references an unknown implementation.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:420](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L420).

### LIR1532

- Field projection must identify its canonical declaring subobject and storage index.
- Field storage index does not identify the resolved declaring field.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1388](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1388), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:432](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L432).

### LIR1533

- Coroutine object receiver must be retained by its canonical frame.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:696](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L696).

## WIRN

### WIRN1001

- Native thunk plan references a function without native binding metadata.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:53](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L53).

### WIRN1002

- NativeInvoke does not resolve to a native declaration.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:104](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L104).

### WIRN1003

- Generic native invocation requires a concrete specialization key.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:112](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L112).

### WIRN1004

- Concrete native thunk has an invalid identity or ABI signature.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:143](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L143).

### WIRN1005

- Concrete native thunk symbol is not unique.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:149](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L149).

### WIRN1006

- NativeInvoke specialization identity does not match its concrete native callee.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:119](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L119).

## WCPP

### WCPP1000

- invalid Lowered WIR: {dynamic context}: {dynamic context}
- {dynamic context}invalid Lowered WIR: {dynamic context}: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:239](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L239), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:3770](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L3770).

### WCPP1100

- unresolved type reached the C++ backend: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:452](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L452).

### WCPP1101

- const-value type has no canonical value

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:459](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L459).

### WCPP1102

- type {dynamic context} requires one argument

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:467](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L467).

### WCPP1103

- dictionary type requires key and value arguments

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:471](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L471).

### WCPP1104

- function type requires a return type

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:475](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L475).

### WCPP1105

- native POD type requires a C++ type binding

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:522](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L522).

### WCPP1106

- object/interface requires canonical hierarchy lowering

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:481](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L481).

### WCPP1107

- enum/flagset requires canonical underlying-type and case layout metadata

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:527](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L527).

### WCPP1108

- global value requires a concrete specialized type

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:537](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L537).

### WCPP1201

- opcode is not implemented by the WIR C++ backend: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:669](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L669).

### WCPP1202

- native invocation requires an adapter thunk not implemented by this backend

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:782](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L782).

### WCPP1203

- constructor identity does not match its canonical argument signature
- constructor '{dynamic context}' requires a callable constructor adapter; only canonical field-wise construction is enabled (operands={dynamic context}, fields={dynamic context})

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:806](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L806), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:828](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L828).

### WCPP1205

- intrinsic family is not implemented by this C++ backend slice: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:739](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L739).

### WCPP1206

- enum/flagset intrinsic is not recognized: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:747](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L747).

### WCPP1207

- enum/flagset constant does not exist in canonical layout: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:754](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L754).

### WCPP1208

- payload variants currently require canonical Option/Result layout

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:770](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L770).

### WCPP1209

- generic call requires a concrete specialized function body

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:728](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L728).

### WCPP1210

- iterator source or operand count is not supported
- iterator projection is not supported: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:690](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L690), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:718](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L718).

### WCPP1211

- cannot construct an object with abstract method contracts

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:794](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L794).

### WCPP1212

- object lifecycle requires a concrete receiver-only body

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:508](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L508), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:517](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L517).

### WCPP1213

- wire ABI requires owned results and forbids ref/view parameters on asynchronous calls

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:611](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L611).

### WCPP1214

- application requires canonical exit-state fields
- worker application stages require a thread-transfer lowering contract

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:548](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L548), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:551](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L551).

### WCPP1215

- behavioral attribute requires a concrete processor object and pinned hook function
- around processors are not valid on asynchronous functions

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:572](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L572), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:579](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L579).

### WCPP1220

- {dynamic context}native-only bridge requires no main and native task sidecars

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:3762](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L3762).

## WBC

### WBC1001

- Module manifest references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:101](../compiler/src/wio_compiler/bytecode/verifier.cpp#L101).

### WBC1002

- String constant references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:106](../compiler/src/wio_compiler/bytecode/verifier.cpp#L106).

### WBC1003

- Constant has an unknown kind

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:108](../compiler/src/wio_compiler/bytecode/verifier.cpp#L108).

### WBC1004

- Type references an invalid name string

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:114](../compiler/src/wio_compiler/bytecode/verifier.cpp#L114).

### WBC1005

- Type argument references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:118](../compiler/src/wio_compiler/bytecode/verifier.cpp#L118).

### WBC1006

- Type extent parameter references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:121](../compiler/src/wio_compiler/bytecode/verifier.cpp#L121).

### WBC1007

- Global ids must be valid and unique

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:181](../compiler/src/wio_compiler/bytecode/verifier.cpp#L181).

### WBC1008

- Global references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:183](../compiler/src/wio_compiler/bytecode/verifier.cpp#L183).

### WBC1009

- Function ids must be valid and unique

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:191](../compiler/src/wio_compiler/bytecode/verifier.cpp#L191).

### WBC1010

- Function references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:194](../compiler/src/wio_compiler/bytecode/verifier.cpp#L194).

### WBC1011

- Block ids must be valid and unique within a function

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:274](../compiler/src/wio_compiler/bytecode/verifier.cpp#L274).

### WBC1012

- Instruction contains an unknown opcode

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:363](../compiler/src/wio_compiler/bytecode/verifier.cpp#L363).

### WBC1013

- Instruction references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:366](../compiler/src/wio_compiler/bytecode/verifier.cpp#L366).

### WBC1014

- Instruction references an invalid constant id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:369](../compiler/src/wio_compiler/bytecode/verifier.cpp#L369).

### WBC1015

- Instruction references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:372](../compiler/src/wio_compiler/bytecode/verifier.cpp#L372).

### WBC1016

- Interpolation segment references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:412](../compiler/src/wio_compiler/bytecode/verifier.cpp#L412).

### WBC1017

- Branch target references a block outside the function

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:418](../compiler/src/wio_compiler/bytecode/verifier.cpp#L418).

### WBC1018

- Nominal base references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:125](../compiler/src/wio_compiler/bytecode/verifier.cpp#L125).

### WBC1019

- Field layout references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:130](../compiler/src/wio_compiler/bytecode/verifier.cpp#L130).

### WBC1020

- Method layout references invalid string, type, or function metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:135](../compiler/src/wio_compiler/bytecode/verifier.cpp#L135).

### WBC1021

- Method parameter references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:139](../compiler/src/wio_compiler/bytecode/verifier.cpp#L139).

### WBC1022

- Dispatch entry on type #{dynamic context} references invalid contract #{dynamic context} or implementation #{dynamic context}

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:146](../compiler/src/wio_compiler/bytecode/verifier.cpp#L146).

### WBC1023

- Type lifecycle or enum metadata contains an invalid id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:165](../compiler/src/wio_compiler/bytecode/verifier.cpp#L165).

### WBC1024

- Enum case references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:169](../compiler/src/wio_compiler/bytecode/verifier.cpp#L169).

### WBC1025

- Native type binding references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:173](../compiler/src/wio_compiler/bytecode/verifier.cpp#L173).

### WBC1026

- Function capture count does not match its capture table

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:196](../compiler/src/wio_compiler/bytecode/verifier.cpp#L196).

### WBC1027

- Closure capture references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:200](../compiler/src/wio_compiler/bytecode/verifier.cpp#L200).

### WBC1028

- Native binding references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:207](../compiler/src/wio_compiler/bytecode/verifier.cpp#L207).

### WBC1029

- Native parameter references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:211](../compiler/src/wio_compiler/bytecode/verifier.cpp#L211).

### WBC1030

- Native template argument references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:265](../compiler/src/wio_compiler/bytecode/verifier.cpp#L265).

### WBC1031

- Coroutine layout does not match its async task return type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:328](../compiler/src/wio_compiler/bytecode/verifier.cpp#L328).

### WBC1032

- Coroutine frame slot is not dense or does not match a live typed value

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:336](../compiler/src/wio_compiler/bytecode/verifier.cpp#L336).

### WBC1033

- Coroutine state references invalid block or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:346](../compiler/src/wio_compiler/bytecode/verifier.cpp#L346).

### WBC1034

- External/template declarations must have no code and defined functions must have code

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:282](../compiler/src/wio_compiler/bytecode/verifier.cpp#L282).

### WBC1035

- Function value ids must be valid, typed, and unique

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:300](../compiler/src/wio_compiler/bytecode/verifier.cpp#L300).

### WBC1036

- Function parameter references an invalid name

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:302](../compiler/src/wio_compiler/bytecode/verifier.cpp#L302).

### WBC1037

- Value-producing instruction requires a unique typed result

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:316](../compiler/src/wio_compiler/bytecode/verifier.cpp#L316).

### WBC1038

- Defined bytecode block cannot be empty

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:355](../compiler/src/wio_compiler/bytecode/verifier.cpp#L355).

### WBC1039

- Every block must end in exactly one final terminator

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:375](../compiler/src/wio_compiler/bytecode/verifier.cpp#L375).

### WBC1040

- Instruction result shape does not match its opcode

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:385](../compiler/src/wio_compiler/bytecode/verifier.cpp#L385).

### WBC1041

- Non-value instruction cannot carry a result type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:389](../compiler/src/wio_compiler/bytecode/verifier.cpp#L389).

### WBC1042

- Pack expansion metadata must align with instruction operands

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:393](../compiler/src/wio_compiler/bytecode/verifier.cpp#L393).

### WBC1043

- Instruction references an invalid callee

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:396](../compiler/src/wio_compiler/bytecode/verifier.cpp#L396).

### WBC1044

- Instruction references an invalid global

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:401](../compiler/src/wio_compiler/bytecode/verifier.cpp#L401).

### WBC1045

- Instruction references an undefined function value

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:406](../compiler/src/wio_compiler/bytecode/verifier.cpp#L406).

### WBC1046

- Branch argument count does not match target parameters

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:424](../compiler/src/wio_compiler/bytecode/verifier.cpp#L424).

### WBC1047

- Branch argument type does not match target parameter

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:433](../compiler/src/wio_compiler/bytecode/verifier.cpp#L433).

### WBC1048

- Control-flow instruction has an invalid operand or target shape

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:446](../compiler/src/wio_compiler/bytecode/verifier.cpp#L446).

### WBC1049

- Function reference does not match a known callable declaration

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:703](../compiler/src/wio_compiler/bytecode/verifier.cpp#L703).

### WBC1050

- Closure creation does not match its callable capture layout

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:732](../compiler/src/wio_compiler/bytecode/verifier.cpp#L732).

### WBC1051

- Indirect call does not match its function value signature

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:765](../compiler/src/wio_compiler/bytecode/verifier.cpp#L765).

### WBC1052

- Dispatch entry does not match an object/interface contract method slot

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:156](../compiler/src/wio_compiler/bytecode/verifier.cpp#L156).

### WBC1054

- Object cast table references a non-nominal or invalid type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:161](../compiler/src/wio_compiler/bytecode/verifier.cpp#L161).

### WBC1055

- Dynamic call does not match its receiver, contract, slot, or declaration

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:803](../compiler/src/wio_compiler/bytecode/verifier.cpp#L803).

### WBC1056

- Object cast or type-test has invalid source or target metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:822](../compiler/src/wio_compiler/bytecode/verifier.cpp#L822).

### WBC1057

- Object identity comparison has invalid operand or operator metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:832](../compiler/src/wio_compiler/bytecode/verifier.cpp#L832).

### WBC1058

- Async bytecode function is missing its coroutine layout

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:350](../compiler/src/wio_compiler/bytecode/verifier.cpp#L350).

### WBC1059

- Cancellation check must precede its matching coroutine suspension

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:842](../compiler/src/wio_compiler/bytecode/verifier.cpp#L842).

### WBC1060

- Coroutine suspension does not match its canonical state and resume edge

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:874](../compiler/src/wio_compiler/bytecode/verifier.cpp#L874).

### WBC1061

- Coroutine resume does not materialize its canonical state payload

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:885](../compiler/src/wio_compiler/bytecode/verifier.cpp#L885).

### WBC1062

- Coroutine completion does not match its async payload type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:898](../compiler/src/wio_compiler/bytecode/verifier.cpp#L898).

### WBC1063

- Executable bytecode cannot contain an unresolved generic constant

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:496](../compiler/src/wio_compiler/bytecode/verifier.cpp#L496).

### WBC1064

- Enum constant does not match a declared enum/flagset case

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:512](../compiler/src/wio_compiler/bytecode/verifier.cpp#L512).

### WBC1065

- Any box does not preserve its materialized payload type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:538](../compiler/src/wio_compiler/bytecode/verifier.cpp#L538).

### WBC1066

- Any test/cast has invalid source, target, or result metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:551](../compiler/src/wio_compiler/bytecode/verifier.cpp#L551).

### WBC1067

- Nullable wrap does not match its payload type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:564](../compiler/src/wio_compiler/bytecode/verifier.cpp#L564).

### WBC1068

- Nullable unwrap does not match its payload result type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:575](../compiler/src/wio_compiler/bytecode/verifier.cpp#L575).

### WBC1069

- Variant operation does not match its option/result layout

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:599](../compiler/src/wio_compiler/bytecode/verifier.cpp#L599).

### WBC1070

- Iterator creation has invalid source or signature metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:640](../compiler/src/wio_compiler/bytecode/verifier.cpp#L640).

### WBC1071

- Iterator operation has invalid cursor or result metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:661](../compiler/src/wio_compiler/bytecode/verifier.cpp#L661).

### WBC1072

- Result operation does not match its canonical result layout

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:690](../compiler/src/wio_compiler/bytecode/verifier.cpp#L690).

### WBC1073

- Native invocation does not match its external ABI declaration

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:491](../compiler/src/wio_compiler/bytecode/verifier.cpp#L491).

### WBC1074

- Native parameter has an invalid ABI mode or no stable type identity ({dynamic context}, type #{dynamic context}, marshalling {dynamic context})
- Native result has an invalid ABI mode or no stable type identity

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:242](../compiler/src/wio_compiler/bytecode/verifier.cpp#L242), [compiler/src/wio_compiler/bytecode/verifier.cpp:261](../compiler/src/wio_compiler/bytecode/verifier.cpp#L261).

### WBC1080

- Template declaration must have an open signature and no runtime binding or coroutine

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:291](../compiler/src/wio_compiler/bytecode/verifier.cpp#L291).

### WBC1100

- Module import contains invalid identity or string metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:909](../compiler/src/wio_compiler/bytecode/verifier.cpp#L909).

### WBC1101

- Module import symbol references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:913](../compiler/src/wio_compiler/bytecode/verifier.cpp#L913).

### WBC1102

- Module export contains invalid identity, symbol, or target metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:922](../compiler/src/wio_compiler/bytecode/verifier.cpp#L922).

### WBC1103

- Module export parameter references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:926](../compiler/src/wio_compiler/bytecode/verifier.cpp#L926).

### WBC1104

- Attribute application contains invalid identity or target metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:934](../compiler/src/wio_compiler/bytecode/verifier.cpp#L934).

### WBC1105

- Attribute argument contains invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:938](../compiler/src/wio_compiler/bytecode/verifier.cpp#L938).

### WBC1106

- Attribute processor contains invalid identity or hook metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:946](../compiler/src/wio_compiler/bytecode/verifier.cpp#L946).

### WBC1107

- Reflection descriptor contains invalid identity or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:952](../compiler/src/wio_compiler/bytecode/verifier.cpp#L952).

### WBC1108

- Reflection generic parameter references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:956](../compiler/src/wio_compiler/bytecode/verifier.cpp#L956).

### WBC1109

- Reflected field contains invalid identity or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:961](../compiler/src/wio_compiler/bytecode/verifier.cpp#L961).

### WBC1110

- Reflected method contains invalid identity or callable metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:967](../compiler/src/wio_compiler/bytecode/verifier.cpp#L967).

### WBC1111

- Reflected case contains invalid identity or string metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:972](../compiler/src/wio_compiler/bytecode/verifier.cpp#L972).

### WBC1112

- System descriptor contains invalid lifecycle metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:979](../compiler/src/wio_compiler/bytecode/verifier.cpp#L979).

### WBC1113

- Application descriptor contains invalid lifecycle metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:988](../compiler/src/wio_compiler/bytecode/verifier.cpp#L988).

### WBC1114

- Application stage contains invalid identity or string metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:992](../compiler/src/wio_compiler/bytecode/verifier.cpp#L992).

### WBC1115

- Application stage run contains invalid target metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:997](../compiler/src/wio_compiler/bytecode/verifier.cpp#L997).

### WBC1116

- Application resource contains invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:1001](../compiler/src/wio_compiler/bytecode/verifier.cpp#L1001).

### WBC1117

- Module lifecycle references an invalid function id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:1010](../compiler/src/wio_compiler/bytecode/verifier.cpp#L1010).

### WBC1118

- SDK call table identity or export cardinality is invalid

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:1013](../compiler/src/wio_compiler/bytecode/verifier.cpp#L1013).

### WBC1119

- Retain/copy-value must preserve the owned type cleanup protocol

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:526](../compiler/src/wio_compiler/bytecode/verifier.cpp#L526).

## WVM

### WVM1000

- The virtual machine has no loaded module
- Timer scheduling requires a loaded VM program

Definitions: [vm/src/machine.cpp:1774](../vm/src/machine.cpp#L1774), [vm/src/machine.cpp:3277](../vm/src/machine.cpp#L3277), [vm/src/machine.cpp:3658](../vm/src/machine.cpp#L3658).

### WVM1001

- Bytecode verification failed ({dynamic context}): {dynamic context}

Definitions: [vm/src/machine.cpp:1648](../vm/src/machine.cpp#L1648).

### WVM1002

- Entry function id is outside the module

Definitions: [vm/src/machine.cpp:1778](../vm/src/machine.cpp#L1778), [vm/src/machine.cpp:3664](../vm/src/machine.cpp#L3664).

### WVM1003

- External function requires the Sprint 20 native bridge

Definitions: [vm/src/machine.cpp:3687](../vm/src/machine.cpp#L3687).

### WVM1004

- Call argument count does not match the function signature

Definitions: [vm/src/machine.cpp:3690](../vm/src/machine.cpp#L3690).

### WVM1005

- VM call depth limit exceeded

Definitions: [vm/src/machine.cpp:3692](../vm/src/machine.cpp#L3692).

### WVM1006

- Function register limit exceeded

Definitions: [vm/src/machine.cpp:3695](../vm/src/machine.cpp#L3695).

### WVM1007

- Instruction cursor escaped its basic block

Definitions: [vm/src/machine.cpp:3832](../vm/src/machine.cpp#L3832).

### WVM1008

- VM instruction limit exceeded

Definitions: [vm/src/machine.cpp:3834](../vm/src/machine.cpp#L3834).

### WVM1009

- Constant result register is invalid
- Default-value result register is invalid
- Array result register is invalid
- Dictionary result register is invalid
- Enum constant result register is invalid
- Enum intrinsic result register is invalid
- Intrinsic result register is invalid
- Any box result register is invalid
- Any type-test result register is invalid
- Any checked-cast result register is invalid
- Nullable wrap result register is invalid
- Nullable unwrap result register is invalid
- Variant-test result register is invalid
- Result error-test register is invalid
- Variant payload result register is invalid
- Range iterator result register is invalid
- Container iterator result register is invalid
- Iterator has-next result register is invalid
- Iterator index result register is invalid
- Iterator reference result register is invalid
- Iterator value result register is invalid
- Aggregate result register is invalid

Definitions: [vm/src/machine.cpp:3948](../vm/src/machine.cpp#L3948), [vm/src/machine.cpp:3974](../vm/src/machine.cpp#L3974), [vm/src/machine.cpp:4174](../vm/src/machine.cpp#L4174), [vm/src/machine.cpp:4215](../vm/src/machine.cpp#L4215), [vm/src/machine.cpp:4307](../vm/src/machine.cpp#L4307), [vm/src/machine.cpp:4393](../vm/src/machine.cpp#L4393), [vm/src/machine.cpp:4490](../vm/src/machine.cpp#L4490), [vm/src/machine.cpp:4508](../vm/src/machine.cpp#L4508), [vm/src/machine.cpp:4935](../vm/src/machine.cpp#L4935), [vm/src/machine.cpp:4952](../vm/src/machine.cpp#L4952), [vm/src/machine.cpp:4966](../vm/src/machine.cpp#L4966), [vm/src/machine.cpp:5019](../vm/src/machine.cpp#L5019), [vm/src/machine.cpp:5036](../vm/src/machine.cpp#L5036), [vm/src/machine.cpp:5047](../vm/src/machine.cpp#L5047), [vm/src/machine.cpp:5060](../vm/src/machine.cpp#L5060), [vm/src/machine.cpp:5081](../vm/src/machine.cpp#L5081), [vm/src/machine.cpp:5086](../vm/src/machine.cpp#L5086), [vm/src/machine.cpp:5098](../vm/src/machine.cpp#L5098), [vm/src/machine.cpp:5123](../vm/src/machine.cpp#L5123), [vm/src/machine.cpp:5144](../vm/src/machine.cpp#L5144), [vm/src/machine.cpp:5216](../vm/src/machine.cpp#L5216), [vm/src/machine.cpp:5243](../vm/src/machine.cpp#L5243), [vm/src/machine.cpp:5309](../vm/src/machine.cpp#L5309), [vm/src/machine.cpp:5312](../vm/src/machine.cpp#L5312), [vm/src/machine.cpp:5734](../vm/src/machine.cpp#L5734).

### WVM1010

- Default value is not implemented for this bytecode type

Definitions: [vm/src/machine.cpp:3972](../vm/src/machine.cpp#L3972).

### WVM1011

- Unary instruction reads an unavailable value

Definitions: [vm/src/machine.cpp:3982](../vm/src/machine.cpp#L3982).

### WVM1012

- Unary operator is invalid for the runtime value kind

Definitions: [vm/src/machine.cpp:3997](../vm/src/machine.cpp#L3997).

### WVM1013

- Binary instruction requires two available values of the same kind

Definitions: [vm/src/machine.cpp:4017](../vm/src/machine.cpp#L4017).

### WVM1014

- Integer division by zero

Definitions: [vm/src/machine.cpp:4037](../vm/src/machine.cpp#L4037), [vm/src/machine.cpp:4090](../vm/src/machine.cpp#L4090).

### WVM1015

- Signed integer division overflow

Definitions: [vm/src/machine.cpp:4039](../vm/src/machine.cpp#L4039).

### WVM1016

- Integer shift count is outside [0, 63]

Definitions: [vm/src/machine.cpp:4051](../vm/src/machine.cpp#L4051), [vm/src/machine.cpp:4102](../vm/src/machine.cpp#L4102).

### WVM1017

- Signed integer binary operator is not implemented

Definitions: [vm/src/machine.cpp:4063](../vm/src/machine.cpp#L4063).

### WVM1018

- Unsigned integer binary operator is not implemented

Definitions: [vm/src/machine.cpp:4106](../vm/src/machine.cpp#L4106).

### WVM1019

- Floating-point binary operator is not implemented

Definitions: [vm/src/machine.cpp:4125](../vm/src/machine.cpp#L4125).

### WVM1020

- String binary operator is not implemented

Definitions: [vm/src/machine.cpp:4138](../vm/src/machine.cpp#L4138).

### WVM1021

- Binary operator is invalid for the runtime value kind

Definitions: [vm/src/machine.cpp:4155](../vm/src/machine.cpp#L4155).

### WVM1022

- Range instruction has an invalid operand shape

Definitions: [vm/src/machine.cpp:5378](../vm/src/machine.cpp#L5378).

### WVM1023

- Range instruction reads incompatible values

Definitions: [vm/src/machine.cpp:5383](../vm/src/machine.cpp#L5383).

### WVM1024

- Range containment requires numeric runtime values

Definitions: [vm/src/machine.cpp:5399](../vm/src/machine.cpp#L5399).

### WVM1025

- Conversion reads an unavailable value

Definitions: [vm/src/machine.cpp:5408](../vm/src/machine.cpp#L5408).

### WVM1026

- Numeric conversion is out of range or invalid

Definitions: [vm/src/machine.cpp:5423](../vm/src/machine.cpp#L5423), [vm/src/machine.cpp:5437](../vm/src/machine.cpp#L5437), [vm/src/machine.cpp:5450](../vm/src/machine.cpp#L5450).

### WVM1027

- Conversion target is not a numeric bytecode type

Definitions: [vm/src/machine.cpp:5455](../vm/src/machine.cpp#L5455).

### WVM1028

- Call target is outside the module

Definitions: [vm/src/machine.cpp:6954](../vm/src/machine.cpp#L6954).

### WVM1029

- Call reads an unavailable argument value
- Dynamic call reads an unavailable argument value

Definitions: [vm/src/machine.cpp:6961](../vm/src/machine.cpp#L6961), [vm/src/machine.cpp:7009](../vm/src/machine.cpp#L7009).

### WVM1030

- Conditional jump requires an available bool value

Definitions: [vm/src/machine.cpp:7200](../vm/src/machine.cpp#L7200).

### WVM1031

- Branch target block does not exist

Definitions: [vm/src/machine.cpp:7207](../vm/src/machine.cpp#L7207).

### WVM1032

- Branch reads an unavailable argument value

Definitions: [vm/src/machine.cpp:7215](../vm/src/machine.cpp#L7215).

### WVM1033

- Return reads an unavailable value

Definitions: [vm/src/machine.cpp:7232](../vm/src/machine.cpp#L7232).

### WVM1034

- Call result register is invalid

Definitions: [vm/src/machine.cpp:3785](../vm/src/machine.cpp#L3785).

### WVM1035

- Execution reached an unreachable instruction

Definitions: [vm/src/machine.cpp:7240](../vm/src/machine.cpp#L7240).

### WVM1036

- Opcode is not implemented by this VM runtime slice: {dynamic context}

Definitions: [vm/src/machine.cpp:7242](../vm/src/machine.cpp#L7242).

### WVM1037

- VM exited without producing a result

Definitions: [vm/src/machine.cpp:7245](../vm/src/machine.cpp#L7245).

### WVM1038

- Local place result register is invalid

Definitions: [vm/src/machine.cpp:5467](../vm/src/machine.cpp#L5467).

### WVM1039

- Global place id or result register is invalid

Definitions: [vm/src/machine.cpp:5475](../vm/src/machine.cpp#L5475).

### WVM1040

- Place write requires an available place and source value

Definitions: [vm/src/machine.cpp:5486](../vm/src/machine.cpp#L5486).

### WVM1041

- Place can only be initialized once

Definitions: [vm/src/machine.cpp:5488](../vm/src/machine.cpp#L5488).

### WVM1042

- Store requires an initialized mutable place

Definitions: [vm/src/machine.cpp:5490](../vm/src/machine.cpp#L5490).

### WVM1043

- VM host cannot export a frame-local borrowed projection
- Load requires an initialized place
- Dynamic dispatch requires an initialized receiver place

Definitions: [vm/src/machine.cpp:3765](../vm/src/machine.cpp#L3765), [vm/src/machine.cpp:5564](../vm/src/machine.cpp#L5564), [vm/src/machine.cpp:6991](../vm/src/machine.cpp#L6991).

### WVM1044

- Borrow requires an available place or object value

Definitions: [vm/src/machine.cpp:5660](../vm/src/machine.cpp#L5660), [vm/src/machine.cpp:5666](../vm/src/machine.cpp#L5666).

### WVM1045

- Copy/retain reads an unavailable value

Definitions: [vm/src/machine.cpp:5854](../vm/src/machine.cpp#L5854).

### WVM1046

- Move requires an initialized place

Definitions: [vm/src/machine.cpp:5866](../vm/src/machine.cpp#L5866).

### WVM1047

- Value cleanup reads an unavailable value

Definitions: [vm/src/machine.cpp:5918](../vm/src/machine.cpp#L5918).

### WVM1048

- Place cleanup requires an initialized place

Definitions: [vm/src/machine.cpp:5936](../vm/src/machine.cpp#L5936).

### WVM1049

- Unicode text constant is not valid UTF-8

Definitions: [vm/src/machine.cpp:3946](../vm/src/machine.cpp#L3946).

### WVM1050

- Unicode text binary operator is not implemented

Definitions: [vm/src/machine.cpp:4152](../vm/src/machine.cpp#L4152).

### WVM1051

- Array creation result type is not an array

Definitions: [vm/src/machine.cpp:4163](../vm/src/machine.cpp#L4163).

### WVM1052

- Array creation reads an unavailable element

Definitions: [vm/src/machine.cpp:4170](../vm/src/machine.cpp#L4170).

### WVM1053

- Array length requires an available array

Definitions: [vm/src/machine.cpp:4182](../vm/src/machine.cpp#L4182).

### WVM1054

- Array index is invalid or outside the array bounds

Definitions: [vm/src/machine.cpp:4195](../vm/src/machine.cpp#L4195).

### WVM1055

- Interpolation segment shape is invalid

Definitions: [vm/src/machine.cpp:4263](../vm/src/machine.cpp#L4263).

### WVM1056

- Interpolation reads an unavailable value

Definitions: [vm/src/machine.cpp:4272](../vm/src/machine.cpp#L4272).

### WVM1057

- Unicode interpolation produced invalid UTF-8

Definitions: [vm/src/machine.cpp:4284](../vm/src/machine.cpp#L4284).

### WVM1058

- Intrinsic call has no available receiver

Definitions: [vm/src/machine.cpp:4404](../vm/src/machine.cpp#L4404).

### WVM1059

- Unicode text index is outside the scalar bounds

Definitions: [vm/src/text_intrinsics.cpp:178](../vm/src/text_intrinsics.cpp#L178).

### WVM1060

- Unicode text slice requires a non-negative start
- Unicode text slice requires a non-negative count

Definitions: [vm/src/text_intrinsics.cpp:166](../vm/src/text_intrinsics.cpp#L166), [vm/src/text_intrinsics.cpp:170](../vm/src/text_intrinsics.cpp#L170).

### WVM1061

- String intrinsic is not implemented: {dynamic context}
- Text intrinsic is not implemented: {dynamic context}
- Container or text intrinsic is not implemented: {dynamic context}

Definitions: [vm/src/machine.cpp:4485](../vm/src/machine.cpp#L4485), [vm/src/machine.cpp:4503](../vm/src/machine.cpp#L4503), [vm/src/machine.cpp:4927](../vm/src/machine.cpp#L4927).

### WVM1062

- Interpolation value has no VM string representation

Definitions: [vm/src/machine.cpp:4275](../vm/src/machine.cpp#L4275).

### WVM1063

- Field place reads an unavailable base

Definitions: [vm/src/machine.cpp:5588](../vm/src/machine.cpp#L5588).

### WVM1064

- Field place base is not initialized

Definitions: [vm/src/machine.cpp:5596](../vm/src/machine.cpp#L5596).

### WVM1065

- Object has no field layout
- Field owner is outside the concrete object hierarchy
- Field projection is outside the aggregate layout

Definitions: [vm/src/machine.cpp:5607](../vm/src/machine.cpp#L5607), [vm/src/machine.cpp:5611](../vm/src/machine.cpp#L5611), [vm/src/machine.cpp:5618](../vm/src/machine.cpp#L5618).

### WVM1066

- Array place requires an available base and non-negative index

Definitions: [vm/src/machine.cpp:5631](../vm/src/machine.cpp#L5631).

### WVM1067

- Array place base is not an initialized array

Definitions: [vm/src/machine.cpp:5639](../vm/src/machine.cpp#L5639).

### WVM1068

- Array place index is outside the array bounds

Definitions: [vm/src/machine.cpp:5649](../vm/src/machine.cpp#L5649).

### WVM1069

- Aggregate construction references an invalid type

Definitions: [vm/src/machine.cpp:5674](../vm/src/machine.cpp#L5674).

### WVM1070

- Aggregate construction kind does not match its result type

Definitions: [vm/src/machine.cpp:5679](../vm/src/machine.cpp#L5679).

### WVM1071

- Component aggregate has more arguments than declared fields
- Component aggregate reads an unavailable field argument
- Aggregate constructor reads an unavailable argument

Definitions: [vm/src/machine.cpp:5742](../vm/src/machine.cpp#L5742), [vm/src/machine.cpp:5747](../vm/src/machine.cpp#L5747), [vm/src/machine.cpp:5774](../vm/src/machine.cpp#L5774).

### WVM1072

- Reference release reads an unavailable value

Definitions: [vm/src/machine.cpp:5891](../vm/src/machine.cpp#L5891).

### WVM1073

- Reference release requires an owned object or callable handle

Definitions: [vm/src/machine.cpp:5896](../vm/src/machine.cpp#L5896).

### WVM1074

- Construction chain references an invalid function
- Construction continuation references an invalid function
- Component construction references an invalid lifecycle function

Definitions: [vm/src/machine.cpp:3801](../vm/src/machine.cpp#L3801), [vm/src/machine.cpp:3815](../vm/src/machine.cpp#L3815), [vm/src/machine.cpp:5782](../vm/src/machine.cpp#L5782).

### WVM1078

- Dictionary creation has an invalid result type or entry shape

Definitions: [vm/src/machine.cpp:4203](../vm/src/machine.cpp#L4203).

### WVM1079

- Dictionary creation reads an unavailable key or value

Definitions: [vm/src/machine.cpp:4210](../vm/src/machine.cpp#L4210).

### WVM1080

- Dictionary key was not found

Definitions: [vm/src/machine.cpp:4225](../vm/src/machine.cpp#L4225), [vm/src/machine.cpp:4253](../vm/src/machine.cpp#L4253), [vm/src/machine.cpp:4766](../vm/src/machine.cpp#L4766).

### WVM1081

- Dictionary place requires an available base and key

Definitions: [vm/src/machine.cpp:4235](../vm/src/machine.cpp#L4235).

### WVM1082

- Dictionary place base is not an initialized dictionary

Definitions: [vm/src/machine.cpp:4243](../vm/src/machine.cpp#L4243).

### WVM1083

- Array Contains reads an unavailable value

Definitions: [vm/src/machine.cpp:4525](../vm/src/machine.cpp#L4525).

### WVM1084

- Dictionary GetOr reads unavailable operands

Definitions: [vm/src/machine.cpp:4776](../vm/src/machine.cpp#L4776).

### WVM1085

- Dictionary TryGet requires a mutable initialized output place

Definitions: [vm/src/machine.cpp:4788](../vm/src/machine.cpp#L4788).

### WVM1086

- Dictionary Set requires a mutable receiver and available operands

Definitions: [vm/src/machine.cpp:4801](../vm/src/machine.cpp#L4801).

### WVM1087

- Dictionary GetOrAdd requires a mutable receiver and operands

Definitions: [vm/src/machine.cpp:4811](../vm/src/machine.cpp#L4811).

### WVM1088

- Dictionary Merge requires another dictionary

Definitions: [vm/src/machine.cpp:4839](../vm/src/machine.cpp#L4839).

### WVM1089

- Dictionary Extend requires a mutable receiver and dictionary

Definitions: [vm/src/machine.cpp:4851](../vm/src/machine.cpp#L4851).

### WVM1090

- Dictionary Clear requires a mutable receiver

Definitions: [vm/src/machine.cpp:4868](../vm/src/machine.cpp#L4868).

### WVM1091

- Dictionary Remove requires a mutable receiver and key

Definitions: [vm/src/machine.cpp:4888](../vm/src/machine.cpp#L4888).

### WVM1092

- Ordered dictionary endpoint requires a non-empty ordered dictionary

Definitions: [vm/src/machine.cpp:4909](../vm/src/machine.cpp#L4909).

### WVM1093

- Ordered dictionary bound lookup has invalid operands

Definitions: [vm/src/machine.cpp:4921](../vm/src/machine.cpp#L4921).

### WVM1094

- Array index search reads an unavailable value

Definitions: [vm/src/machine.cpp:4539](../vm/src/machine.cpp#L4539).

### WVM1095

- Array endpoint intrinsic requires a non-empty array

Definitions: [vm/src/machine.cpp:4569](../vm/src/machine.cpp#L4569).

### WVM1096

- Array intrinsic index is outside the array bounds

Definitions: [vm/src/machine.cpp:4578](../vm/src/machine.cpp#L4578).

### WVM1097

- Array GetOr reads an unavailable fallback

Definitions: [vm/src/machine.cpp:4587](../vm/src/machine.cpp#L4587).

### WVM1098

- Array Slice requires non-negative start and count operands

Definitions: [vm/src/machine.cpp:4602](../vm/src/machine.cpp#L4602).

### WVM1099

- Array Take/Skip requires a non-negative count

Definitions: [vm/src/machine.cpp:4610](../vm/src/machine.cpp#L4610).

### WVM1100

- Array Concat requires another array

Definitions: [vm/src/machine.cpp:4619](../vm/src/machine.cpp#L4619).

### WVM1101

- Array Join requires a string separator

Definitions: [vm/src/machine.cpp:4633](../vm/src/machine.cpp#L4633).

### WVM1102

- Array Join requires string elements

Definitions: [vm/src/machine.cpp:4639](../vm/src/machine.cpp#L4639).

### WVM1103

- Array Push requires a mutable receiver and available value

Definitions: [vm/src/machine.cpp:4651](../vm/src/machine.cpp#L4651).

### WVM1104

- Array Pop requires a mutable non-empty array

Definitions: [vm/src/machine.cpp:4658](../vm/src/machine.cpp#L4658).

### WVM1105

- Array Insert index is invalid or receiver is not mutable

Definitions: [vm/src/machine.cpp:4666](../vm/src/machine.cpp#L4666).

### WVM1106

- Array RemoveAt index is invalid or receiver is not mutable

Definitions: [vm/src/machine.cpp:4673](../vm/src/machine.cpp#L4673).

### WVM1107

- Array Remove requires a mutable receiver and available value

Definitions: [vm/src/machine.cpp:4680](../vm/src/machine.cpp#L4680).

### WVM1108

- Array Clear requires a mutable receiver

Definitions: [vm/src/machine.cpp:4687](../vm/src/machine.cpp#L4687).

### WVM1109

- Array Extend requires a mutable receiver and another array

Definitions: [vm/src/machine.cpp:4695](../vm/src/machine.cpp#L4695).

### WVM1110

- Array Reserve requires a mutable receiver and valid capacity

Definitions: [vm/src/machine.cpp:4702](../vm/src/machine.cpp#L4702).

### WVM1111

- Array ShrinkToFit requires a mutable receiver

Definitions: [vm/src/machine.cpp:4708](../vm/src/machine.cpp#L4708).

### WVM1112

- Array Fill requires a mutable receiver and available value

Definitions: [vm/src/machine.cpp:4716](../vm/src/machine.cpp#L4716).

### WVM1113

- Array Reverse requires a mutable receiver

Definitions: [vm/src/machine.cpp:4723](../vm/src/machine.cpp#L4723).

### WVM1114

- Array Sort requires a mutable array of orderable values

Definitions: [vm/src/machine.cpp:4730](../vm/src/machine.cpp#L4730).

### WVM1115

- Array Sorted requires orderable values

Definitions: [vm/src/machine.cpp:4737](../vm/src/machine.cpp#L4737).

### WVM1116

- String intrinsic receiver is not a string

Definitions: [vm/src/string_intrinsics.cpp:242](../vm/src/string_intrinsics.cpp#L242).

### WVM1117

- String Contains requires a string or byte-sized char

Definitions: [vm/src/string_intrinsics.cpp:256](../vm/src/string_intrinsics.cpp#L256).

### WVM1118

- String prefix/suffix query requires a string

Definitions: [vm/src/string_intrinsics.cpp:262](../vm/src/string_intrinsics.cpp#L262).

### WVM1119

- String index query requires a string or byte-sized char

Definitions: [vm/src/string_intrinsics.cpp:278](../vm/src/string_intrinsics.cpp#L278).

### WVM1120

- String endpoint intrinsic requires a non-empty string

Definitions: [vm/src/string_intrinsics.cpp:285](../vm/src/string_intrinsics.cpp#L285).

### WVM1121

- String indexing requires a non-negative index

Definitions: [vm/src/string_intrinsics.cpp:294](../vm/src/string_intrinsics.cpp#L294).

### WVM1122

- String index is outside the byte bounds

Definitions: [vm/src/string_intrinsics.cpp:300](../vm/src/string_intrinsics.cpp#L300).

### WVM1123

- String slicing requires a non-negative index or count
- String Slice requires a non-negative count

Definitions: [vm/src/string_intrinsics.cpp:309](../vm/src/string_intrinsics.cpp#L309), [vm/src/string_intrinsics.cpp:322](../vm/src/string_intrinsics.cpp#L322).

### WVM1124

- String replacement requires old and new string values

Definitions: [vm/src/string_intrinsics.cpp:344](../vm/src/string_intrinsics.cpp#L344), [vm/src/string_intrinsics.cpp:540](../vm/src/string_intrinsics.cpp#L540).

### WVM1125

- String Repeat count exceeds runtime limits

Definitions: [vm/src/string_intrinsics.cpp:353](../vm/src/string_intrinsics.cpp#L353).

### WVM1126

- String Split requires a separator

Definitions: [vm/src/string_intrinsics.cpp:367](../vm/src/string_intrinsics.cpp#L367).

### WVM1127

- String padding requires a width and optional byte-sized char

Definitions: [vm/src/string_intrinsics.cpp:412](../vm/src/string_intrinsics.cpp#L412).

### WVM1128

- String integer conversion has invalid input or base
- String integer conversion failed
- String unsigned conversion is out of range
- String signed conversion is out of range
- String integer conversion base is invalid

Definitions: [vm/src/string_intrinsics.cpp:196](../vm/src/string_intrinsics.cpp#L196), [vm/src/string_intrinsics.cpp:201](../vm/src/string_intrinsics.cpp#L201), [vm/src/string_intrinsics.cpp:212](../vm/src/string_intrinsics.cpp#L212), [vm/src/string_intrinsics.cpp:220](../vm/src/string_intrinsics.cpp#L220), [vm/src/string_intrinsics.cpp:430](../vm/src/string_intrinsics.cpp#L430).

### WVM1129

- String f32 conversion failed
- String floating-point conversion failed

Definitions: [vm/src/string_intrinsics.cpp:447](../vm/src/string_intrinsics.cpp#L447), [vm/src/string_intrinsics.cpp:455](../vm/src/string_intrinsics.cpp#L455).

### WVM1130

- String boolean conversion failed

Definitions: [vm/src/string_intrinsics.cpp:467](../vm/src/string_intrinsics.cpp#L467).

### WVM1131

- String Append requires a string suffix

Definitions: [vm/src/string_intrinsics.cpp:481](../vm/src/string_intrinsics.cpp#L481), [vm/src/string_intrinsics.cpp:504](../vm/src/string_intrinsics.cpp#L504).

### WVM1132

- String Push requires a byte-sized char

Definitions: [vm/src/string_intrinsics.cpp:489](../vm/src/string_intrinsics.cpp#L489), [vm/src/string_intrinsics.cpp:511](../vm/src/string_intrinsics.cpp#L511).

### WVM1133

- String Insert requires an index and fragment

Definitions: [vm/src/string_intrinsics.cpp:519](../vm/src/string_intrinsics.cpp#L519).

### WVM1134

- String Erase requires index and count operands

Definitions: [vm/src/string_intrinsics.cpp:527](../vm/src/string_intrinsics.cpp#L527).

### WVM1135

- String mutation requires a mutable receiver

Definitions: [vm/src/string_intrinsics.cpp:231](../vm/src/string_intrinsics.cpp#L231).

### WVM1136

- Text intrinsic receiver is not Unicode text

Definitions: [vm/src/text_intrinsics.cpp:152](../vm/src/text_intrinsics.cpp#L152).

### WVM1137

- Unicode text query requires another text value

Definitions: [vm/src/text_intrinsics.cpp:185](../vm/src/text_intrinsics.cpp#L185).

### WVM1138

- Grapheme slicing requires non-negative start and count values

Definitions: [vm/src/text_intrinsics.cpp:204](../vm/src/text_intrinsics.cpp#L204).

### WVM1139

- Function reference requires a known function and callable type

Definitions: [vm/src/machine.cpp:5956](../vm/src/machine.cpp#L5956).

### WVM1140

- Function reference result register is invalid

Definitions: [vm/src/machine.cpp:5958](../vm/src/machine.cpp#L5958).

### WVM1141

- Closure creation does not match a known callable capture layout

Definitions: [vm/src/machine.cpp:5967](../vm/src/machine.cpp#L5967).

### WVM1142

- Closure creation reads an unavailable capture value

Definitions: [vm/src/machine.cpp:5975](../vm/src/machine.cpp#L5975).

### WVM1143

- Reference capture requires a live borrowed place

Definitions: [vm/src/machine.cpp:5984](../vm/src/machine.cpp#L5984).

### WVM1144

- Retained-self capture requires a live component or object

Definitions: [vm/src/machine.cpp:6002](../vm/src/machine.cpp#L6002).

### WVM1145

- Closure creation contains an unknown capture kind

Definitions: [vm/src/machine.cpp:6007](../vm/src/machine.cpp#L6007).

### WVM1146

- Closure result register is invalid
- Native callable result register is invalid

Definitions: [vm/src/machine.cpp:6011](../vm/src/machine.cpp#L6011), [vm/src/machine.cpp:6038](../vm/src/machine.cpp#L6038).

### WVM1147

- Indirect call requires an available function value

Definitions: [vm/src/machine.cpp:6019](../vm/src/machine.cpp#L6019).

### WVM1148

- Indirect call target is outside the module

Definitions: [vm/src/machine.cpp:6044](../vm/src/machine.cpp#L6044).

### WVM1149

- Indirect call lost a closure capture

Definitions: [vm/src/machine.cpp:6052](../vm/src/machine.cpp#L6052).

### WVM1150

- Native indirect call reads an unavailable argument
- Indirect call reads an unavailable argument value

Definitions: [vm/src/machine.cpp:6030](../vm/src/machine.cpp#L6030), [vm/src/machine.cpp:6059](../vm/src/machine.cpp#L6059).

### WVM1151

- Object cast requires a live object value

Definitions: [vm/src/machine.cpp:6088](../vm/src/machine.cpp#L6088).

### WVM1152

Definition: Upcast target is incompatible with the concrete object type.

Definitions: [vm/src/machine.cpp:6090](../vm/src/machine.cpp#L6090).

### WVM1153

Definition: Checked object cast failed for the concrete object type.

Definitions: [vm/src/machine.cpp:6090](../vm/src/machine.cpp#L6090).

### WVM1154

- Object cast result register is invalid

Definitions: [vm/src/machine.cpp:6102](../vm/src/machine.cpp#L6102).

### WVM1155

- Object type-test result register is invalid

Definitions: [vm/src/machine.cpp:6112](../vm/src/machine.cpp#L6112).

### WVM1156

- Object identity comparison requires two operands

Definitions: [vm/src/machine.cpp:6119](../vm/src/machine.cpp#L6119).

### WVM1157

- Object identity comparison reads an unavailable value

Definitions: [vm/src/machine.cpp:6123](../vm/src/machine.cpp#L6123).

### WVM1158

- Object identity comparison requires object or null operands

Definitions: [vm/src/machine.cpp:6130](../vm/src/machine.cpp#L6130).

### WVM1159

- Object identity comparison has an invalid comparison operator

Definitions: [vm/src/machine.cpp:6137](../vm/src/machine.cpp#L6137).

### WVM1160

- Object identity comparison result register is invalid

Definitions: [vm/src/machine.cpp:6139](../vm/src/machine.cpp#L6139).

### WVM1161

- Dynamic dispatch requires a live object receiver

Definitions: [vm/src/machine.cpp:6995](../vm/src/machine.cpp#L6995).

### WVM1162

- Concrete object type has no implementation for the requested method slot

Definitions: [vm/src/machine.cpp:6999](../vm/src/machine.cpp#L6999).

### WVM1163

- Dynamic dispatch resolved outside the module function table

Definitions: [vm/src/machine.cpp:7001](../vm/src/machine.cpp#L7001).

### WVM1164

- Wait requires an async task value

Definitions: [vm/src/machine.cpp:3375](../vm/src/machine.cpp#L3375).

### WVM1165

- Async task was cancelled during machine shutdown
- Async task was cancelled
- Awaited async task was cancelled
- Async task was cancelled before its continuation resumed
- Async task was cancelled at a cooperative suspension point

Definitions: [vm/src/machine.cpp:1615](../vm/src/machine.cpp#L1615), [vm/src/machine.cpp:2908](../vm/src/machine.cpp#L2908), [vm/src/machine.cpp:2939](../vm/src/machine.cpp#L2939), [vm/src/machine.cpp:3389](../vm/src/machine.cpp#L3389), [vm/src/machine.cpp:3404](../vm/src/machine.cpp#L3404), [vm/src/machine.cpp:3433](../vm/src/machine.cpp#L3433), [vm/src/machine.cpp:3445](../vm/src/machine.cpp#L3445), [vm/src/machine.cpp:3465](../vm/src/machine.cpp#L3465), [vm/src/machine.cpp:3534](../vm/src/machine.cpp#L3534), [vm/src/machine.cpp:3596](../vm/src/machine.cpp#L3596), [vm/src/machine.cpp:7033](../vm/src/machine.cpp#L7033), [vm/src/machine.cpp:7069](../vm/src/machine.cpp#L7069).

### WVM1166

- Async task is already running or has an invalid state

Definitions: [vm/src/machine.cpp:3408](../vm/src/machine.cpp#L3408).

### WVM1167

- Async task lost a captured argument

Definitions: [vm/src/machine.cpp:3417](../vm/src/machine.cpp#L3417), [vm/src/machine.cpp:3419](../vm/src/machine.cpp#L3419).

### WVM1168

- Async indirect-call result register is invalid

Definitions: [vm/src/machine.cpp:6070](../vm/src/machine.cpp#L6070).

### WVM1169

- Async call result register is invalid

Definitions: [vm/src/machine.cpp:6968](../vm/src/machine.cpp#L6968).

### WVM1170

- Async dynamic-call result register is invalid

Definitions: [vm/src/machine.cpp:7016](../vm/src/machine.cpp#L7016).

### WVM1171

- Coroutine suspension does not match its function state table

Definitions: [vm/src/machine.cpp:7042](../vm/src/machine.cpp#L7042).

### WVM1172

- Coroutine suspension state does not match its resume edge

Definitions: [vm/src/machine.cpp:7047](../vm/src/machine.cpp#L7047).

### WVM1173

- Coroutine await requires an async task operand

Definitions: [vm/src/machine.cpp:7056](../vm/src/machine.cpp#L7056).

### WVM1174

- Coroutine suspension carries an unsupported async operation

Definitions: [vm/src/machine.cpp:7132](../vm/src/machine.cpp#L7132).

### WVM1175

- Coroutine resume block does not exist

Definitions: [vm/src/machine.cpp:3561](../vm/src/machine.cpp#L3561), [vm/src/machine.cpp:7136](../vm/src/machine.cpp#L7136).

### WVM1176

- Coroutine resume has no matching suspended payload

Definitions: [vm/src/machine.cpp:7160](../vm/src/machine.cpp#L7160).

### WVM1177

- Coroutine resume result register is invalid

Definitions: [vm/src/machine.cpp:7162](../vm/src/machine.cpp#L7162).

### WVM1178

- Enum constant does not match its canonical type metadata

Definitions: [vm/src/machine.cpp:4301](../vm/src/machine.cpp#L4301).

### WVM1179

- Enum intrinsic has no available receiver or target type
- Enum intrinsic target metadata is invalid
- Enum intrinsic receiver is not an integer value

Definitions: [vm/src/machine.cpp:4317](../vm/src/machine.cpp#L4317), [vm/src/machine.cpp:4322](../vm/src/machine.cpp#L4322), [vm/src/machine.cpp:4334](../vm/src/machine.cpp#L4334).

### WVM1180

- Flagset intrinsic requires an integer mask operand

Definitions: [vm/src/machine.cpp:4369](../vm/src/machine.cpp#L4369).

### WVM1181

- Enum or flagset intrinsic is not implemented: {dynamic context}

Definitions: [vm/src/machine.cpp:4385](../vm/src/machine.cpp#L4385).

### WVM1182

- Any boxing requires an available payload

Definitions: [vm/src/machine.cpp:4944](../vm/src/machine.cpp#L4944).

### WVM1183

- Any test or cast requires a live boxed value

Definitions: [vm/src/machine.cpp:5007](../vm/src/machine.cpp#L5007).

### WVM1184

- Checked any cast cannot recover a payload from null
- Checked any cast failed for the requested target type
- Checked any cast cannot borrow a non-object payload

Definitions: [vm/src/machine.cpp:4964](../vm/src/machine.cpp#L4964), [vm/src/machine.cpp:5024](../vm/src/machine.cpp#L5024), [vm/src/machine.cpp:5032](../vm/src/machine.cpp#L5032).

### WVM1185

- Nullable wrapping requires an available payload

Definitions: [vm/src/machine.cpp:5045](../vm/src/machine.cpp#L5045).

### WVM1186

- Nullable unwrap requires an available value

Definitions: [vm/src/machine.cpp:5056](../vm/src/machine.cpp#L5056).

### WVM1187

- Nullable value does not contain a payload

Definitions: [vm/src/machine.cpp:5058](../vm/src/machine.cpp#L5058).

### WVM1188

- Variant operation requires canonical option/result storage

Definitions: [vm/src/machine.cpp:5074](../vm/src/machine.cpp#L5074).

### WVM1189

- Result does not contain a success value

Definitions: [vm/src/machine.cpp:5093](../vm/src/machine.cpp#L5093).

### WVM1190

- Variant payload is outside its canonical field layout

Definitions: [vm/src/machine.cpp:5096](../vm/src/machine.cpp#L5096).

### WVM1191

- Iterator creation result type is not an iterator

Definitions: [vm/src/machine.cpp:5108](../vm/src/machine.cpp#L5108).

### WVM1192

- Range iterator requires three compatible integer values

Definitions: [vm/src/machine.cpp:5116](../vm/src/machine.cpp#L5116).

### WVM1193

- Range iterator step cannot be zero

Definitions: [vm/src/machine.cpp:5120](../vm/src/machine.cpp#L5120).

### WVM1194

- Container iterator source does not match its selector
- Container iterator has an invalid operand shape

Definitions: [vm/src/machine.cpp:5131](../vm/src/machine.cpp#L5131), [vm/src/machine.cpp:5142](../vm/src/machine.cpp#L5142).

### WVM1195

- Container iterator step must be a positive integer

Definitions: [vm/src/machine.cpp:5138](../vm/src/machine.cpp#L5138).

### WVM1196

- Iterator selector is not implemented: {dynamic context}

Definitions: [vm/src/machine.cpp:5147](../vm/src/machine.cpp#L5147).

### WVM1197

- Iterator operation requires a live iterator value

Definitions: [vm/src/machine.cpp:5159](../vm/src/machine.cpp#L5159).

### WVM1198

- Iterator state is incompatible with its source
- Range iterator state is incomplete
- Range iterator state is not integer-compatible
- Container iterator source is unavailable

Definitions: [vm/src/machine.cpp:5214](../vm/src/machine.cpp#L5214), [vm/src/machine.cpp:5328](../vm/src/machine.cpp#L5328), [vm/src/machine.cpp:5355](../vm/src/machine.cpp#L5355), [vm/src/machine.cpp:5361](../vm/src/machine.cpp#L5361).

### WVM1199

- Range iterator has no current value
- Container iterator has no current value
- Iterator current value is unavailable

Definitions: [vm/src/machine.cpp:5229](../vm/src/machine.cpp#L5229), [vm/src/machine.cpp:5238](../vm/src/machine.cpp#L5238), [vm/src/machine.cpp:5293](../vm/src/machine.cpp#L5293).

### WVM1200

- Array iterator field projection is invalid

Definitions: [vm/src/machine.cpp:5265](../vm/src/machine.cpp#L5265), [vm/src/machine.cpp:5270](../vm/src/machine.cpp#L5270).

### WVM1201

- Dictionary iterator projection is invalid

Definitions: [vm/src/machine.cpp:5289](../vm/src/machine.cpp#L5289).

### WVM1202

- Result propagation requires a canonical error payload and target type
- Result propagation target has no canonical result layout
- Result propagation target fields are unavailable

Definitions: [vm/src/machine.cpp:7176](../vm/src/machine.cpp#L7176), [vm/src/machine.cpp:7181](../vm/src/machine.cpp#L7181), [vm/src/machine.cpp:7186](../vm/src/machine.cpp#L7186).

### WVM1204

- Async task failure lost its terminal-state race
- Async task completion lost its terminal-state race

Definitions: [vm/src/machine.cpp:3448](../vm/src/machine.cpp#L3448), [vm/src/machine.cpp:3468](../vm/src/machine.cpp#L3468).

### WVM1205

- VM execution was aborted by the debug observer

Definitions: [vm/src/machine.cpp:3858](../vm/src/machine.cpp#L3858).

### WVM1206

- VM debug observer threw across the runtime boundary

Definitions: [vm/src/machine.cpp:3851](../vm/src/machine.cpp#L3851).

### WVM1207

- VM worker callable threw across the runtime boundary
- VM executor owner dispatch failed
- VM executor continuation threw across the runtime boundary

Definitions: [vm/src/machine.cpp:3344](../vm/src/machine.cpp#L3344), [vm/src/machine.cpp:3509](../vm/src/machine.cpp#L3509), [vm/src/machine.cpp:3641](../vm/src/machine.cpp#L3641), [vm/src/machine.cpp:3646](../vm/src/machine.cpp#L3646).

### WVM1208

- Await continuation executor rejected the resumed VM frame
- Coroutine executor switch has no runnable target
- Coroutine executor queue rejected its continuation

Definitions: [vm/src/machine.cpp:3581](../vm/src/machine.cpp#L3581), [vm/src/machine.cpp:7145](../vm/src/machine.cpp#L7145), [vm/src/machine.cpp:7149](../vm/src/machine.cpp#L7149).

### WVM1209

- Await continuation lost its VM frame
- Coroutine await has no owning async task

Definitions: [vm/src/machine.cpp:3551](../vm/src/machine.cpp#L3551), [vm/src/machine.cpp:7084](../vm/src/machine.cpp#L7084).

### WVM1210

- {dynamic context}VM destructor threw while unwinding: {dynamic context}
- VM destructor threw a non-standard exception while unwinding
- VM await continuation dispatch failed

Definitions: [vm/src/machine.cpp:1524](../vm/src/machine.cpp#L1524), [vm/src/machine.cpp:1529](../vm/src/machine.cpp#L1529), [vm/src/machine.cpp:7114](../vm/src/machine.cpp#L7114).

### WVM1211

- Dictionary mutation cleanup failed while running a destructor
- Reference cleanup failed while running a destructor
- Value cleanup failed while running a destructor
- Place cleanup failed while running a destructor
- Native callback cleanup failed while running a destructor
- Native copy-back cleanup failed while running a destructor

Definitions: [vm/src/machine.cpp:4930](../vm/src/machine.cpp#L4930), [vm/src/machine.cpp:5521](../vm/src/machine.cpp#L5521), [vm/src/machine.cpp:5902](../vm/src/machine.cpp#L5902), [vm/src/machine.cpp:5923](../vm/src/machine.cpp#L5923), [vm/src/machine.cpp:5945](../vm/src/machine.cpp#L5945), [vm/src/machine.cpp:6725](../vm/src/machine.cpp#L6725), [vm/src/machine.cpp:6817](../vm/src/machine.cpp#L6817).

### WVM1212

- Async argument cleanup failed while running a destructor

Definitions: [vm/src/machine.cpp:3454](../vm/src/machine.cpp#L3454), [vm/src/machine.cpp:3630](../vm/src/machine.cpp#L3630).

### WVM1220

- Native invocation references an invalid function declaration
- Native invocation target has no native binding metadata
- Native function is not registered: {dynamic context}

Definitions: [vm/src/machine.cpp:6146](../vm/src/machine.cpp#L6146), [vm/src/machine.cpp:6149](../vm/src/machine.cpp#L6149), [vm/src/machine.cpp:6233](../vm/src/machine.cpp#L6233).

### WVM1221

- Native invocation argument count does not match its ABI plan

Definitions: [vm/src/machine.cpp:6151](../vm/src/machine.cpp#L6151).

### WVM1222

- Native callable has caller-thread affinity
- Native callable failed with ABI status {dynamic context}
- Native invocation '{dynamic context}' failed: {dynamic context}

Definitions: [vm/src/machine.cpp:2149](../vm/src/machine.cpp#L2149), [vm/src/machine.cpp:2187](../vm/src/machine.cpp#L2187), [vm/src/machine.cpp:6826](../vm/src/machine.cpp#L6826).

### WVM1223

- Native invocation reads an unavailable argument value
- Native ref/view argument does not reference a VM place
- Native ref/view argument references unavailable or immutable storage
- Native argument conversion failed: {dynamic context}
- Aliased native references disagree on their ABI value type
- Native value argument references unavailable storage
- Native callback argument has an invalid callable ABI shape
- Native container projection has an incompatible value type
- Native container object projection has no contract identity
- Native projection key conversion failed: {dynamic context}
- Native projection key has no contract identity
- Native sequence projection has an incompatible element type
- Native sequence object projection has no contract identity
- Overlapping native field reference has an incompatible producer layout: {dynamic context} (field type {dynamic context}, argument type {dynamic context}, identity {dynamic context})
- Native fixed-array reference requires a containing POD projection
- Native thunk does not advertise task-source input support; rebuild its producer
- Native callable result requires a task-source-aware producer

Definitions: [vm/src/machine.cpp:2142](../vm/src/machine.cpp#L2142), [vm/src/machine.cpp:3967](../vm/src/machine.cpp#L3967), [vm/src/machine.cpp:5704](../vm/src/machine.cpp#L5704), [vm/src/machine.cpp:5712](../vm/src/machine.cpp#L5712), [vm/src/machine.cpp:5726](../vm/src/machine.cpp#L5726), [vm/src/machine.cpp:6338](../vm/src/machine.cpp#L6338), [vm/src/machine.cpp:6345](../vm/src/machine.cpp#L6345), [vm/src/machine.cpp:6348](../vm/src/machine.cpp#L6348), [vm/src/machine.cpp:6368](../vm/src/machine.cpp#L6368), [vm/src/machine.cpp:6383](../vm/src/machine.cpp#L6383), [vm/src/machine.cpp:6399](../vm/src/machine.cpp#L6399), [vm/src/machine.cpp:6405](../vm/src/machine.cpp#L6405), [vm/src/machine.cpp:6444](../vm/src/machine.cpp#L6444), [vm/src/machine.cpp:6503](../vm/src/machine.cpp#L6503), [vm/src/machine.cpp:6512](../vm/src/machine.cpp#L6512), [vm/src/machine.cpp:6533](../vm/src/machine.cpp#L6533), [vm/src/machine.cpp:6538](../vm/src/machine.cpp#L6538), [vm/src/machine.cpp:6562](../vm/src/machine.cpp#L6562), [vm/src/machine.cpp:6571](../vm/src/machine.cpp#L6571), [vm/src/machine.cpp:6652](../vm/src/machine.cpp#L6652), [vm/src/machine.cpp:6680](../vm/src/machine.cpp#L6680), [vm/src/machine.cpp:6705](../vm/src/machine.cpp#L6705), [vm/src/machine.cpp:6835](../vm/src/machine.cpp#L6835).

### WVM1224

- Native any payload refresh failed: {dynamic context}
- Native any payload is unsupported or absent from its descriptor closure
- Native any shared cell changed its payload identity
- VM async service result register is invalid
- Native mutable-reference copy-back failed: {dynamic context}
- Native borrowed result has an invalid reference contract
- Native producer did not advertise projected borrowed results
- Native borrowed result does not name a compatible caller reference
- Native borrowed result register is invalid
- Native task identity recovery failed: {dynamic context}
- Native task result register is invalid
- Native async result register is invalid
- Native result conversion failed: {dynamic context}
- Native result register is invalid

Definitions: [vm/src/machine.cpp:4992](../vm/src/machine.cpp#L4992), [vm/src/machine.cpp:4994](../vm/src/machine.cpp#L4994), [vm/src/machine.cpp:4998](../vm/src/machine.cpp#L4998), [vm/src/machine.cpp:6235](../vm/src/machine.cpp#L6235), [vm/src/machine.cpp:6830](../vm/src/machine.cpp#L6830), [vm/src/machine.cpp:6856](../vm/src/machine.cpp#L6856), [vm/src/machine.cpp:6861](../vm/src/machine.cpp#L6861), [vm/src/machine.cpp:6897](../vm/src/machine.cpp#L6897), [vm/src/machine.cpp:6900](../vm/src/machine.cpp#L6900), [vm/src/machine.cpp:6910](../vm/src/machine.cpp#L6910), [vm/src/machine.cpp:6913](../vm/src/machine.cpp#L6913), [vm/src/machine.cpp:6931](../vm/src/machine.cpp#L6931), [vm/src/machine.cpp:6943](../vm/src/machine.cpp#L6943), [vm/src/machine.cpp:6946](../vm/src/machine.cpp#L6946).

### WVM1225

- Native async result has no registered task bridge
- Invocation-scoped native callback borrows cannot be captured
- Native callback environment contains a borrowed capture
- Native producer does not advertise consumed argument ownership
- Native callbacks require scalar, string, text, qualified native POD, hosted object/interface, registered container or qualified value ref/view arguments; unsupported callback type #{dynamic context}
- Overlapping native container references require qualified projections (map keys, bool proxies or unsupported field path)
- Native producer does not support dictionary/container projections
- Native producer does not support sequence reference projections
- Native producer does not support sequence/POD field projections
- Native object/field aliasing requires synchronized projections
- Native producer does not support overlapping POD reference projections
- Native async result requires a registered task bridge and one payload
- Native async payload cannot be represented by the VM ABI bridge

Definitions: [vm/src/machine.cpp:1235](../vm/src/machine.cpp#L1235), [vm/src/machine.cpp:5987](../vm/src/machine.cpp#L5987), [vm/src/machine.cpp:6021](../vm/src/machine.cpp#L6021), [vm/src/machine.cpp:6340](../vm/src/machine.cpp#L6340), [vm/src/machine.cpp:6418](../vm/src/machine.cpp#L6418), [vm/src/machine.cpp:6429](../vm/src/machine.cpp#L6429), [vm/src/machine.cpp:6477](../vm/src/machine.cpp#L6477), [vm/src/machine.cpp:6501](../vm/src/machine.cpp#L6501), [vm/src/machine.cpp:6557](../vm/src/machine.cpp#L6557), [vm/src/machine.cpp:6559](../vm/src/machine.cpp#L6559), [vm/src/machine.cpp:6615](../vm/src/machine.cpp#L6615), [vm/src/machine.cpp:6646](../vm/src/machine.cpp#L6646), [vm/src/machine.cpp:6920](../vm/src/machine.cpp#L6920), [vm/src/machine.cpp:6924](../vm/src/machine.cpp#L6924).

### WVM1226

- Native task completion subscription failed with ABI status {dynamic context}
- Native task completion raised an internal bridge failure
- Native task completion failed with ABI status {dynamic context}
- Native task result failed: {dynamic context}
- Native task result conversion failed: {dynamic context}
- Native task cancellation failed with ABI status {dynamic context}

Definitions: [vm/src/machine.cpp:1263](../vm/src/machine.cpp#L1263), [vm/src/machine.cpp:1286](../vm/src/machine.cpp#L1286), [vm/src/machine.cpp:1321](../vm/src/machine.cpp#L1321), [vm/src/machine.cpp:1340](../vm/src/machine.cpp#L1340), [vm/src/machine.cpp:1355](../vm/src/machine.cpp#L1355), [vm/src/machine.cpp:1391](../vm/src/machine.cpp#L1391).

### WVM1240

- Generic declaration requires a concrete function specialization

Definitions: [vm/src/machine.cpp:1781](../vm/src/machine.cpp#L1781), [vm/src/machine.cpp:3684](../vm/src/machine.cpp#L3684).

### WVM1241

Definition: VM object-layout indexing failed while loading the verified bytecode module. The detailed layout message is supplied by ObjectLayouts::build.

Detailed message: dynamically supplied by the reporting path.
Message expression: `std::move(layoutError)`.

Definitions: [vm/src/machine.cpp:1657](../vm/src/machine.cpp#L1657).

## Native, SDK and std status/error enums

Values below are current implementation values. Only explicitly ABI-versioned enums are wire contracts; do not rely on incidental SDK exception ordinals as a stable ABI. OS native codes retain their platform meaning and user Result codes retain their author-defined domain.

`WioResultError` carries domain, portable code, native OS code and message separately. The same integer in two domains is not the same error. `none`/`Ok` denotes success/no error; `Pending`/`NotReady`/`TimedOut` is task state or an observation outcome and must not be conflated with a faulted payload. ConsoleErrorDomain and WioResultDomain classify provenance, not failure reasons.

### ErrorCode — sdk/include/wio_sdk.h

Source: [sdk/include/wio_sdk.h](../sdk/include/wio_sdk.h#L87).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `InvalidArgument` | 0 | The caller supplied an invalid argument to an SDK operation. |
| `LibraryOpenFailed` | 1 | The platform loader could not open the requested module library. |
| `SymbolLookupFailed` | 2 | A required exported symbol could not be resolved in the library. |
| `ApiUnavailable` | 3 | The requested module API or optional sidecar capability is unavailable. |
| `InvalidApiDescriptor` | 4 | Module/sidecar descriptor version, size or required pointers are invalid. |
| `ExportNotFound` | 5 | The requested exported entry is absent. |
| `TypeNotFound` | 6 | The requested exported/reflected type is absent. |
| `CommandNotFound` | 7 | The requested exported command is absent. |
| `EventNotFound` | 8 | The requested exported event is absent. |
| `EventHookNotFound` | 9 | The requested event hook is absent. |
| `FieldNotFound` | 10 | The requested field descriptor is absent. |
| `MethodNotFound` | 11 | The requested callable method descriptor is absent. |
| `FieldNotWritable` | 12 | The requested field is readonly or has no writable bridge. |
| `SignatureMismatch` | 13 | Requested type/signature/shape differs from the advertised bridge contract. |
| `InvokeFailed` | 14 | The exported/native invocation failed or returned an invalid result. |
| `LifecycleFailed` | 15 | A module/application lifecycle operation failed. |
| `StaleBinding` | 16 | A binding belongs to a retired/unloaded generation and cannot be used. |
| `ReloadFailed` | 17 | Staging or replacing the module during reload failed. |
| `IoFailure` | 18 | An SDK file/I/O operation failed. |

### WioNativeAbiStatus — sdk/include/wio_native_abi.h

Source: [sdk/include/wio_native_abi.h](../sdk/include/wio_native_abi.h#L12).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_NATIVE_ABI_OK` | 0 | ok |
| `WIO_NATIVE_ABI_INVALID_ARGUMENT` | 1 | invalid argument |
| `WIO_NATIVE_ABI_TYPE_MISMATCH` | 2 | type mismatch |
| `WIO_NATIVE_ABI_EXCEPTION` | 3 | exception |
| `WIO_NATIVE_ABI_PANIC` | 4 | panic |
| `WIO_NATIVE_ABI_WRONG_THREAD` | 5 | wrong thread |
| `WIO_NATIVE_ABI_STALE_HANDLE` | 6 | stale handle |
| `WIO_NATIVE_ABI_NOT_READY` | 7 | not ready |
| `WIO_NATIVE_ABI_CANCELLED` | 8 | cancelled |

### WioInvokeStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L179).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_INVOKE_OK` | 0 | ok |
| `WIO_INVOKE_EXPORT_NOT_FOUND` | 1 | export not found |
| `WIO_INVOKE_BAD_ARGUMENTS` | 2 | bad arguments |
| `WIO_INVOKE_TYPE_MISMATCH` | 3 | type mismatch |
| `WIO_INVOKE_RESULT_REQUIRED` | 4 | result required |
| `WIO_INVOKE_NOT_CALLABLE` | 5 | not callable |

### WioCallbackStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L473).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_CALLBACK_OK` | 0 | ok |
| `WIO_CALLBACK_BAD_ARGUMENTS` | 1 | bad arguments |
| `WIO_CALLBACK_TYPE_MISMATCH` | 2 | type mismatch |
| `WIO_CALLBACK_RESULT_REQUIRED` | 3 | result required |
| `WIO_CALLBACK_FAULTED` | 4 | faulted |

### WioApplicationStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L536).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_APPLICATION_OK` | 0 | ok |
| `WIO_APPLICATION_EXIT_REQUESTED` | 1 | exit requested |
| `WIO_APPLICATION_ALREADY_CLOSED` | 2 | already closed |
| `WIO_APPLICATION_INVALID_STATE` | 3 | invalid state |
| `WIO_APPLICATION_FAULTED` | 4 | faulted |
| `WIO_APPLICATION_WRONG_THREAD` | 5 | wrong thread |

### WioAsyncTaskStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L608).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_ASYNC_TASK_PENDING` | 0 | pending |
| `WIO_ASYNC_TASK_READY` | 1 | ready |
| `WIO_ASYNC_TASK_CANCELLED` | 2 | cancelled |
| `WIO_ASYNC_TASK_FAULTED` | 3 | faulted |

### WioAsyncOperationStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L616).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_ASYNC_OK` | 0 | ok |
| `WIO_ASYNC_TIMED_OUT` | 1 | timed out |
| `WIO_ASYNC_NOT_READY` | 2 | not ready |
| `WIO_ASYNC_CANCELLED` | 3 | cancelled |
| `WIO_ASYNC_FAULTED` | 4 | faulted |
| `WIO_ASYNC_BAD_ARGUMENTS` | 5 | bad arguments |
| `WIO_ASYNC_TYPE_MISMATCH` | 6 | type mismatch |

### WioResultDomain — sdk/include/wio_values.h

Source: [sdk/include/wio_values.h](../sdk/include/wio_values.h#L301).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `None` | 0 | none |
| `Console` | 1 | console |
| `Io` | 2 | io |
| `FileSystem` | 3 | file system |
| `Runtime` | 4 | runtime |
| `Custom` | 5 | custom |

### WriteError — runtime/include/io_errors.h

Source: [runtime/include/io_errors.h](../runtime/include/io_errors.h#L6).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_file` | 1 | null file |
| `null_data` | 2 | null data |
| `size_overflow` | 3 | size overflow |
| `io_error` | 4 | io error |
| `partial_write` | 5 | partial write |

### ReadError — runtime/include/io_errors.h

Source: [runtime/include/io_errors.h](../runtime/include/io_errors.h#L16).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_file` | 1 | null file |
| `invalid_argument` | 2 | invalid argument |
| `eof` | 3 | eof |
| `io_error` | 4 | io error |
| `partial_read` | 5 | partial read |
| `platform_error` | 6 | platform error |

### ParseError — runtime/include/std_convert.h

Source: [runtime/include/std_convert.h](../runtime/include/std_convert.h#L10).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `empty` | 1 | empty |
| `invalid_format` | 2 | invalid format |
| `trailing_characters` | 3 | trailing characters |
| `out_of_range` | 4 | out of range |
| `invalid_base` | 5 | invalid base |

### ErrorCode — runtime/include/std_fs.h

Source: [runtime/include/std_fs.h](../runtime/include/std_fs.h#L12).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `None` | 0 | none |
| `InvalidPath` | 1 | invalid path |
| `NotFound` | 2 | not found |
| `AlreadyExists` | 3 | already exists |
| `NotDirectory` | 4 | not directory |
| `IsDirectory` | 5 | is directory |
| `PermissionDenied` | 6 | permission denied |
| `Io` | 7 | io |
| `Unsupported` | 8 | unsupported |
| `Unknown` | 9 | unknown |

### FileError — runtime/include/std_io.h

Source: [runtime/include/std_io.h](../runtime/include/std_io.h#L128).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_path` | 1 | null path |
| `empty_path` | 2 | empty path |
| `invalid_path` | 3 | invalid path |
| `invalid_utf8` | 4 | invalid utf8 |
| `allocation_failed` | 5 | allocation failed |
| `invalid_argument` | 6 | invalid argument |
| `invalid_mode` | 7 | invalid mode |
| `unsupported_mode` | 8 | unsupported mode |
| `already_open` | 9 | already open |
| `not_open` | 10 | not open |
| `open_failed` | 11 | open failed |
| `close_failed` | 12 | close failed |
| `sync_failed` | 13 | sync failed |
| `seek_failed` | 14 | seek failed |
| `tell_failed` | 15 | tell failed |
| `size_failed` | 16 | size failed |

### ProcessError — runtime/include/std_process.h

Source: [runtime/include/std_process.h](../runtime/include/std_process.h#L10).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `empty_program` | 1 | empty program |
| `invalid_working_directory` | 2 | invalid working directory |
| `launch_failed` | 3 | launch failed |
| `pipe_failed` | 4 | pipe failed |
| `process_closed` | 5 | process closed |
| `wait_failed` | 6 | wait failed |
| `terminate_failed` | 7 | terminate failed |
| `cancelled` | 8 | cancelled |

### RegexError — runtime/include/std_regex.h

Source: [runtime/include/std_regex.h](../runtime/include/std_regex.h#L10).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `invalid_pattern` | 1 | invalid pattern |
| `runtime_error` | 2 | runtime error |
| `limit_exceeded` | 3 | limit exceeded |

### ConsoleStatus — runtime/include/std_console.h

Source: [runtime/include/std_console.h](../runtime/include/std_console.h#L28).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `Ok` | 0 | ok |
| `Unsupported` | 1 | unsupported |
| `Unavailable` | 2 | unavailable |
| `InvalidArgument` | 3 | invalid argument |
| `InvalidRange` | 4 | invalid range |
| `NullArgument` | 5 | null argument |
| `InputRedirected` | 6 | input redirected |
| `OutputRedirected` | 7 | output redirected |
| `ErrorRedirected` | 8 | error redirected |
| `IoError` | 9 | io error |
| `PartialIo` | 10 | partial io |
| `EndOfFile` | 11 | end of file |
| `EncodingError` | 12 | encoding error |
| `FormatError` | 13 | format error |
| `PermissionDenied` | 14 | permission denied |
| `PlatformError` | 15 | platform error |
| `UnknownError` | 16 | unknown error |

### ConsoleErrorDomain — runtime/include/std_console.h

Source: [runtime/include/std_console.h](../runtime/include/std_console.h#L49).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `None` | 0 | none |
| `Generic` | 1 | generic |
| `Stdio` | 2 | stdio |
| `Win32` | 3 | win32 |
| `Posix` | 4 | posix |
| `Terminal` | 5 | terminal |
| `Formatting` | 6 | formatting |

### TaskStatus — std/async.wio

Source: [std/async.wio](../std/async.wio#L17).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `pending` | 0 | pending |
| `ready` | 1 | ready |
| `failed` | 2 | failed |
| `cancelled` | 3 | cancelled |

### ChannelSendStatus — std/async.wio

Source: [std/async.wio](../std/async.wio#L330).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `accepted` | 0 | accepted |
| `backpressured` | 1 | backpressured |
| `closed` | 2 | closed |

### ChannelReceiveStatus — std/async.wio

Source: [std/async.wio](../std/async.wio#L336).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `available` | 0 | available |
| `empty` | 1 | empty |
| `closed` | 2 | closed |

### Status — std/console.wio

Source: [std/console.wio](../std/console.wio#L12).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `Ok` | 0 | ok |
| `Unsupported` | 1 | unsupported |
| `Unavailable` | 2 | unavailable |
| `InvalidArgument` | 3 | invalid argument |
| `InvalidRange` | 4 | invalid range |
| `NullArgument` | 5 | null argument |
| `InputRedirected` | 6 | input redirected |
| `OutputRedirected` | 7 | output redirected |
| `ErrorRedirected` | 8 | error redirected |
| `IoError` | 9 | io error |
| `PartialIo` | 10 | partial io |
| `EndOfFile` | 11 | end of file |
| `EncodingError` | 12 | encoding error |
| `FormatError` | 13 | format error |
| `PermissionDenied` | 14 | permission denied |
| `PlatformError` | 15 | platform error |
| `UnknownError` | 16 | unknown error |

### ErrorDomain — std/console.wio

Source: [std/console.wio](../std/console.wio#L35).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `None` | 0 | none |
| `Generic` | 1 | generic |
| `Stdio` | 2 | stdio |
| `Win32` | 3 | win32 |
| `Posix` | 4 | posix |
| `Terminal` | 5 | terminal |
| `Formatting` | 6 | formatting |

### Error — std/fs.wio

Source: [std/fs.wio](../std/fs.wio#L11).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `fs_none` | 0 | fs none |
| `fs_invalid_path` | 1 | fs invalid path |
| `fs_not_found` | 2 | fs not found |
| `fs_already_exists` | 3 | fs already exists |
| `fs_not_directory` | 4 | fs not directory |
| `fs_is_directory` | 5 | fs is directory |
| `fs_permission_denied` | 6 | fs permission denied |
| `fs_io` | 7 | fs io |
| `fs_unsupported` | 8 | fs unsupported |
| `fs_unknown` | 9 | fs unknown |
| `fs_cancelled` | 10 | fs cancelled |

### FileError — std/io.wio

Source: [std/io.wio](../std/io.wio#L40).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_path` | 1 | null path |
| `empty_path` | 2 | empty path |
| `invalid_path` | 3 | invalid path |
| `invalid_utf8` | 4 | invalid utf8 |
| `allocation_failed` | 5 | allocation failed |
| `invalid_argument` | 6 | invalid argument |
| `invalid_mode` | 7 | invalid mode |
| `unsupported_mode` | 8 | unsupported mode |
| `already_open` | 9 | already open |
| `not_open` | 10 | not open |
| `open_failed` | 11 | open failed |
| `close_failed` | 12 | close failed |
| `sync_failed` | 13 | sync failed |
| `seek_failed` | 14 | seek failed |
| `tell_failed` | 15 | tell failed |
| `size_failed` | 16 | size failed |

### ReadError — std/io.wio

Source: [std/io.wio](../std/io.wio#L72).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_file` | 1 | null file |
| `invalid_argument` | 2 | invalid argument |
| `eof` | 3 | eof |
| `io_error` | 4 | io error |
| `partial_read` | 5 | partial read |
| `platform_error` | 6 | platform error |

### WriteError — std/io.wio

Source: [std/io.wio](../std/io.wio#L85).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_file` | 1 | null file |
| `null_data` | 2 | null data |
| `size_overflow` | 3 | size overflow |
| `io_error` | 4 | io error |
| `partial_write` | 5 | partial write |

### ParseError — std/convert.wio

Source: [std/convert.wio](../std/convert.wio#L14).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `empty` | 1 | empty |
| `invalid_format` | 2 | invalid format |
| `trailing_characters` | 3 | trailing characters |
| `out_of_range` | 4 | out of range |
| `invalid_base` | 5 | invalid base |

### Error — std/process.wio

Source: [std/process.wio](../std/process.wio#L12).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `empty_program` | 1 | empty program |
| `invalid_working_directory` | 2 | invalid working directory |
| `launch_failed` | 3 | launch failed |
| `pipe_failed` | 4 | pipe failed |
| `process_closed` | 5 | process closed |
| `wait_failed` | 6 | wait failed |
| `terminate_failed` | 7 | terminate failed |
| `cancelled` | 8 | cancelled |

### ResultDomain — std/result.wio

Source: [std/result.wio](../std/result.wio#L9).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `console` | 1 | console |
| `io` | 2 | io |
| `fs` | 3 | fs |
| `runtime` | 4 | runtime |
| `custom` | 5 | custom |

### Error — std/regex.wio

Source: [std/regex.wio](../std/regex.wio#L10).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `invalid_pattern` | 1 | invalid pattern |
| `runtime_error` | 2 | runtime error |
| `limit_exceeded` | 3 | limit exceeded |

## Standard Result error construction sites

This section inventories MakeResultError calls in committed std sources. The std sections deliberately read Git HEAD, not unrelated uncommitted user edits; commit std changes before regenerating their reference. Custom numeric codes are module-local conventions, not globally unique diagnostic IDs. Variable codes/messages forward an upstream error; platform nativeCode values remain platform-defined. Function declarations are not error sites.

| Source | Domain | Code expression | Message / definition template |
| --- | --- | --- | --- |
| [std/binary.wio:52](../std/binary.wio#L52) | `std::ResultDomain::custom` | `1` | Binary frame header is truncated. |
| [std/binary.wio:58](../std/binary.wio#L58) | `std::ResultDomain::custom` | `2` | Binary frame magic is invalid. |
| [std/binary.wio:65](../std/binary.wio#L65) | `std::ResultDomain::custom` | `3` | Binary frame payload length is invalid. |
| [std/binary.wio:72](../std/binary.wio#L72) | `std::ResultDomain::custom` | `4` | Binary frame contains malformed trailing data. |
| [std/builders.wio:29](../std/builders.wio#L29) | `std::ResultDomain::io` | `code.Value()` | Forwarded/dynamic message: message |
| [std/compression.wio:25](../std/compression.wio#L25) | `std::ResultDomain::custom` | `1` | Malformed RLE payload. |
| [std/compression.wio:33](../std/compression.wio#L33) | `std::ResultDomain::custom` | `2` | RLE output limit exceeded. |
| [std/config.wio:32](../std/config.wio#L32) | `std::ResultDomain::custom` | `1` | Invalid INI assignment at line ${lineNumber}. |
| [std/config.wio:39](../std/config.wio#L39) | `std::ResultDomain::custom` | `1` | Empty INI key at line ${lineNumber}. |
| [std/console.wio:423](../std/console.wio#L423) | `ResultDomain::console` | `StatusCodeValue(error.status)` | Forwarded/dynamic message: message |
| [std/convert.wio:145](../std/convert.wio#L145) | `std::ResultDomain::custom` | `ErrorCode(error)` | Forwarded/dynamic message: message |
| [std/csv.wio:41](../std/csv.wio#L41) | `std::ResultDomain::custom` | `1` | CSV contains an unterminated quoted field. |
| [std/fs.wio:129](../std/fs.wio#L129) | `std::ResultDomain::fs` | `code` | Forwarded/dynamic message: message |
| [std/fs.wio:238](../std/fs.wio#L238) | `std::ResultDomain::fs` | `code` | Forwarded/dynamic message: message |
| [std/io.wio:353](../std/io.wio#L353) | `ResultDomain::io` | `FileErrorCode(error)` | Forwarded/dynamic message: message |
| [std/io.wio:357](../std/io.wio#L357) | `ResultDomain::io` | `ReadErrorCode(error)` | Forwarded/dynamic message: message |
| [std/io.wio:361](../std/io.wio#L361) | `ResultDomain::io` | `WriteErrorCode(error)` | Forwarded/dynamic message: message |
| [std/json.wio:76](../std/json.wio#L76) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:83](../std/json.wio#L83) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:90](../std/json.wio#L90) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:97](../std/json.wio#L97) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:104](../std/json.wio#L104) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:111](../std/json.wio#L111) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:118](../std/json.wio#L118) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:125](../std/json.wio#L125) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:132](../std/json.wio#L132) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:139](../std/json.wio#L139) | `std::ResultDomain::custom` | `3` | JSON value is not an integer. |
| [std/json.wio:427](../std/json.wio#L427) | `std::ResultDomain::custom` | `2` | JSON input exceeds the configured byte limit. |
| [std/json.wio:431](../std/json.wio#L431) | `std::ResultDomain::custom` | `2` | JSON input is not valid UTF-8. |
| [std/json.wio:440](../std/json.wio#L440) | `std::ResultDomain::custom` | `1` | Forwarded/dynamic message: self.failure |
| [std/net.wio:26](../std/net.wio#L26) | `std::ResultDomain::runtime` | `1` | URI requires a scheme and authority. |
| [std/net.wio:40](../std/net.wio#L40) | `std::ResultDomain::runtime` | `1` | URI host cannot be empty. |
| [std/net.wio:47](../std/net.wio#L47) | `std::ResultDomain::runtime` | `1` | Invalid bracketed IPv6 authority. |
| [std/net.wio:144](../std/net.wio#L144) | `std::ResultDomain::runtime` | `1` | Forwarded/dynamic message: message |
| [std/os.wio:102](../std/os.wio#L102) | `std::ResultDomain::runtime` | `1` | File watcher stopped unexpectedly. |
| [std/path.wio:70](../std/path.wio#L70) | `std::ResultDomain::fs` | `code` | Forwarded/dynamic message: message |
| [std/process.wio:159](../std/process.wio#L159) | `std::ResultDomain::runtime` | `ErrorCode(error)` | Forwarded/dynamic message: finalMessage |
| [std/regex.wio:139](../std/regex.wio#L139) | `std::ResultDomain::runtime` | `3` | Regular-expression input limit exceeded. |
| [std/regex.wio:143](../std/regex.wio#L143) | `std::ResultDomain::runtime` | `4` | Potentially unsafe regular-expression pattern rejected. |
| [std/regex.wio:160](../std/regex.wio#L160) | `std::ResultDomain::runtime` | `ErrorCode(error)` | Forwarded/dynamic message: finalMessage |
| [std/result.wio:229](../std/result.wio#L229) | `domain` | `code` | Forwarded/dynamic message: message |
| [std/result.wio:233](../std/result.wio#L233) | `domain` | `code` | Forwarded/dynamic message: message |
| [std/unicode.wio:75](../std/unicode.wio#L75) | `std::ResultDomain::runtime` | `5` | Unicode normalization failed: invalid UTF-8 or form. |
| [std/unicode.wio:109](../std/unicode.wio#L109) | `std::ResultDomain::runtime` | `1` | Invalid UTF-8 text. |
| [std/unicode.wio:120](../std/unicode.wio#L120) | `std::ResultDomain::runtime` | `3` | Invalid UTF-16 at code-unit index ${errorIndex}. |
| [std/unicode.wio:137](../std/unicode.wio#L137) | `std::ResultDomain::runtime` | `4` | Invalid UTF-32 scalar at index ${errorIndex}. |
| [std/unicode.wio:154](../std/unicode.wio#L154) | `std::ResultDomain::runtime` | `1` | Invalid UTF-8 at byte ${errorOffset}. |
| [std/unicode.wio:167](../std/unicode.wio#L167) | `std::ResultDomain::runtime` | `2` | Invalid Unicode scalar at index ${errorIndex}. |
| [std/unicode.wio:178](../std/unicode.wio#L178) | `std::ResultDomain::runtime` | `2` | Invalid Unicode scalar value. |
