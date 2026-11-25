# Adds the design kit's VST3 resources to a built bundle (run after JUCE writes moduleinfo.json):
#   - PlugIn.ico and desktop.ini at the bundle root, so Windows Explorer shows the icon
#   - the host snapshots, named with the processor's class ID (Contents/Resources/Snapshots)
#
#   cmake -DBUNDLE=path/to/De-Saturator.vst3 -DBRAND=path/to/design/installers/vst3 -P Vst3Resources.cmake

if(NOT EXISTS "${BUNDLE}" OR NOT EXISTS "${BRAND}")
    message(FATAL_ERROR "Vst3Resources: missing BUNDLE (${BUNDLE}) or BRAND (${BRAND})")
endif()

if(WIN32)
    file(COPY "${BRAND}/PlugIn.ico" "${BRAND}/desktop.ini" DESTINATION "${BUNDLE}")
endif()

set(moduleinfo "${BUNDLE}/Contents/Resources/moduleinfo.json")
if(NOT EXISTS "${moduleinfo}")
    message(WARNING "Vst3Resources: no moduleinfo.json yet; skipping the snapshots")
    return()
endif()

# The first class is the processor ("Audio Module Class"). moduleinfo.json isn't strict JSON
# (trailing commas), so read the ID with a regular expression.
file(READ "${moduleinfo}" info)
string(REGEX MATCH "\"CID\": *\"([0-9A-Fa-f]+)\"" match "${info}")
if(NOT CMAKE_MATCH_1)
    message(WARNING "Vst3Resources: no class ID in moduleinfo.json; skipping the snapshots")
    return()
endif()
set(cid "${CMAKE_MATCH_1}")

set(snapshots "${BUNDLE}/Contents/Resources/Snapshots")
file(MAKE_DIRECTORY "${snapshots}")
file(COPY_FILE "${BRAND}/Resources/Snapshots/PROCESSOR-UID_snapshot.png" "${snapshots}/${cid}_snapshot.png")
file(COPY_FILE "${BRAND}/Resources/Snapshots/PROCESSOR-UID_snapshot_2.0x.png" "${snapshots}/${cid}_snapshot_2.0x.png")
