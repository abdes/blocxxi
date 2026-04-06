# CMake Helper Notes

## Embedded JSON Schemas

Use `cmake/JsonSchemaHelpers.cmake` to embed JSON schema files into generated C++ headers at build time.

The public API is:

```cmake
include(JsonSchemaHelpers)

nova_embed_json_schemas(
  TARGET <target>
  OUTPUT_HEADER <path-to-generated-header>
  NAMESPACE <c++ namespace>
  INCLUDE_BASE_DIR <include-root-added-to-target>
  CHUNK_SIZE <max chars per raw-string chunk>
  SCHEMAS
    <symbol_name_1> <schema_file_1>
    <symbol_name_2> <schema_file_2>
)
```

Arguments:

1. `TARGET` is the module target that consumes the generated header.
2. `OUTPUT_HEADER` is where the generated `.h` is written.
3. `NAMESPACE` is the C++ namespace used in the generated header.
4. `INCLUDE_BASE_DIR` is added `BEFORE` to the target include dirs.
5. `CHUNK_SIZE` controls raw-string chunk size for compiler compatibility.
6. `SCHEMAS` is an alternating `<symbol_name> <schema_file>` list.

Implementation split:

1. `JsonSchemaHelpers.cmake` is configure-time API and build graph wiring.
2. `GenerateEmbeddedJsonSchemas.cmake` is build-time code generation backend.

Why split:

1. Correct incremental rebuild behavior when schema files change.
2. Cleaner `add_custom_command` invocation without large inline script logic.
3. More robust escaping/quoting across generators.

Current example:

1. Source schemas:
   - `src/Schemas/oxygen.import-manifest.schema.json`
   - `src/Schemas/oxygen.input.schema.json`
   - `src/Schemas/oxygen.input-action.schema.json`
   - `src/Schemas/oxygen.physics-sidecar.schema.json`
2. Generated header path: `out/<build>/generated/Internal/ImportManifest_schema.h`
3. Include in C++ remains: `<Nova/Cooker/Import/Internal/ImportManifest_schema.h>`

Minimal module usage example:

```cmake
include(JsonSchemaHelpers)

nova_embed_json_schemas(
  TARGET ${META_MODULE_TARGET}
  OUTPUT_HEADER
    "${CMAKE_BINARY_DIR}/generated/Internal/ImportManifest_schema.h"
  NAMESPACE "nova::content::import"
  INCLUDE_BASE_DIR
    "${CMAKE_BINARY_DIR}/generated"
  CHUNK_SIZE 8192
  SCHEMAS
    kImportManifestSchema
    "${CMAKE_CURRENT_SOURCE_DIR}/Import/Schemas/oxygen.import-manifest.schema.json"
    kInputSchema
    "${CMAKE_CURRENT_SOURCE_DIR}/Import/Schemas/oxygen.input.schema.json"
    kInputActionSchema
    "${CMAKE_CURRENT_SOURCE_DIR}/Import/Schemas/oxygen.input-action.schema.json"
    kPhysicsSidecarSchema
    "${CMAKE_CURRENT_SOURCE_DIR}/Import/Schemas/oxygen.physics-sidecar.schema.json"
)
```

Conventions:

1. Keep schema source files under the owning module, for example `Import/Schemas/`.
2. Keep module control in module `CMakeLists.txt` by calling `nova_embed_json_schemas(...)` there.
3. Keep generation logic centralized under `cmake/`.
4. Prefer stable symbol names (`k...Schema`) and canonical schema filenames (`*.schema.json`).
