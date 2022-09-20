# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

## [1.0.1](http://github.com/abdes/asap/compare/v0.10.0...v1.0.1) (2022-09-20)

### Bug Fixes

* work around and resolve gcc warnings ([69c7433](http://github.com/abdes/asap/commit/69c7433ac655c5455542fec7be8312a0095de581))

## [1.0.0](http://github.com/abdes/asap/compare/v0.10.0...v1.0.0) (2022-09-20)

### Features

* add the `version-info` tool to print the project info ([cb228e8](http://github.com/abdes/asap/commit/cb228e8af73fbf063371e4c597f757bf5e9a4b75))
* enhance configure logs with project/module nesting hierarchy ([f6c13f2](http://github.com/abdes/asap/commit/f6c13f2a08c89cac57fb2f0dd857c8f382e50e7b))
* implement robust project-wide formatting ([afcaebe](http://github.com/abdes/asap/commit/afcaebe544fc03684ae2f85d8507b1f4571d989b))

### Bug Fixes

* [#10](http://github.com/abdes/asap/issues/10) no more template export header ([dd8ffd5](http://github.com/abdes/asap/commit/dd8ffd5a8f36340963349c7ebcb7c1713c2f880a))
* [#11](http://github.com/abdes/asap/issues/11) refactor compiler options management ([78ae493](http://github.com/abdes/asap/commit/78ae4933f2e263a55f6537e66347c6b11a24b961))
* [#12](http://github.com/abdes/asap/issues/12) disable used-but-marked-unused ([6d42d83](http://github.com/abdes/asap/commit/6d42d83bfdd16123f05a69726058dc5f103143be))
* [#13](http://github.com/abdes/asap/issues/13) move "caexcludepath" to dev-windows and exclude CPM cache ([0571714](http://github.com/abdes/asap/commit/0571714e9436bfec26d6450b5bc37f2a5f478a55))
* [#14](http://github.com/abdes/asap/issues/14) upgrade CPM to 0.35.6 ([695414b](http://github.com/abdes/asap/commit/695414b8e66d4d42d7ef3aaef3c6a4b8399d16c2))
* [#16](http://github.com/abdes/asap/issues/16) use CMAKE_CURRENT_SOURCE_DIR instead of CMAKE_SOURCE_DIR for cmake includes ([4ac6928](http://github.com/abdes/asap/commit/4ac6928fc2a0bf806bbcaa3bea898b5ff018a164))
* [#17](http://github.com/abdes/asap/issues/17) git should not be required ([2c76104](http://github.com/abdes/asap/commit/2c761046d0801f643aa0215d34f2795ff0093dfc))
* [#18](http://github.com/abdes/asap/issues/18) enforce end of line to LF ([943ae47](http://github.com/abdes/asap/commit/943ae479e09de999c324a9cfe3bbf8d688d255a3))
* [#19](http://github.com/abdes/asap/issues/19) use generator expressions instead of CMAKE_BUILD_TYPE ([857d299](http://github.com/abdes/asap/commit/857d2997d4ec6c879036e10234b8baf907e91089))
* [#20](http://github.com/abdes/asap/issues/20) local install should use CMAKE_INSTALL_PREFIX to set variables ([2e1f1d4](http://github.com/abdes/asap/commit/2e1f1d49baff64dbf47dbbda234886ad2dfdbf1c))
* [#20](http://github.com/abdes/asap/issues/20) use CMAKE_INSTALL_PREFIX to set variables ([2fffd96](http://github.com/abdes/asap/commit/2fffd96392114993bbb72e3f614725f867d61ab1))
* [#9](http://github.com/abdes/asap/issues/9) remove no longer used function ([5a7416f](http://github.com/abdes/asap/commit/5a7416f9563aae303d68ca2bb878fef97fbb7130))
* add missing diagnostic push pragma at end of file ([1d59bc5](http://github.com/abdes/asap/commit/1d59bc5d02b6ac96d2784d529995c6cb254a7265))
* contract mode definition should only be added when not testing asap_contract ([c6d5e34](http://github.com/abdes/asap/commit/c6d5e342e7a74236bb1b006be1e0d6bfe956a51a))
* don't build cryptopp shared lib as we only use the static one ([7e40b1e](http://github.com/abdes/asap/commit/7e40b1eb187d0e5afa3c10af3e3c5dba8168ab50))
* eliminate compiler warnings ([5c4ee9c](http://github.com/abdes/asap/commit/5c4ee9cd58ffe554236f5f9e853a838d963033b1))
* generated `version.h` should follow project naming ([329bcdf](http://github.com/abdes/asap/commit/329bcdfc8cb9ba4782d0cbf4b3f21ad677307644))
* inline function to avoid compiler warning ([3e985dd](http://github.com/abdes/asap/commit/3e985ddff4cc807da645fe78d61d1f7679689fcb))
* install master project generated header files ([3c5c162](http://github.com/abdes/asap/commit/3c5c1628b3c920e52200f7e14ecde2346b78a6f4))
* **macos:** check clang diagnostics support ([79808b0](http://github.com/abdes/asap/commit/79808b0058c407b9ae6d66f55e9fd71ca9360426))
* restore test setup deleted by mistake ([cec7b9d](http://github.com/abdes/asap/commit/cec7b9d92481d1480c54610892cbfd954b9e0068))
* top level install not working properly ([4ac4a31](http://github.com/abdes/asap/commit/4ac4a31001a2ab73764e3d9fe3f279b1e7b25aee))
* use cmake-format extension default behavior ([a5d5c5e](http://github.com/abdes/asap/commit/a5d5c5eae39e4d3d0094c00848cfe777d331a219))
* use correct path for the version include file ([7997b68](http://github.com/abdes/asap/commit/7997b68eeef236e6940b0ba168c79dbdd21b34ad))
* wrong variable used for target name ([4ecd2bb](http://github.com/abdes/asap/commit/4ecd2bbfa896547e77cdbf7ba1c535f80125eef4))
* wrong variable used of target name ([04b5343](http://github.com/abdes/asap/commit/04b5343ae541bd6d4f5ae1c1fa2eb85b93e0b5a3))

### Documentation

* add example output from version-info tool ([3a5515e](http://github.com/abdes/asap/commit/3a5515e74b0b0e5c06ba7e4500f7572a3bc4450f))
* update after new formatting system ([082e513](http://github.com/abdes/asap/commit/082e5134fd7d1cd03cc06218e10d5cf978b22409))

## 0.10.0 (2022-08-04)

Initial version after refactoring to use [asap](https://github.com/abdes/asap).
