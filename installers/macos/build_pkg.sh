#!/bin/bash
# Builds the macOS installer (VST3 and AU), with art from design/installers/macos.
#   installers/macos/build_pkg.sh VERSION path/to/DeSaturator_artefacts/Release output-folder
#
# Signing, all optional. Releases so far are NOT signed with a Developer ID, so on download macOS
# will ask the user to confirm opening the package (right-click > Open, or Privacy & Security).
#   DESAT_APP_IDENTITY        "Developer ID Application: ..." signs the plugins (hardened runtime).
#                             Without it they are signed ad hoc.
#   DESAT_INSTALLER_IDENTITY  "Developer ID Installer: ..." signs the package.
#   DESAT_NOTARY_PROFILE      a notarytool keychain profile: notarises and staples the package.
set -euo pipefail

version="$1"
artefacts="$2"
output="$3"
root="$(cd "$(dirname "$0")/../.." && pwd)"
art="$root/design/installers/macos/pkg"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

app_identity="${DESAT_APP_IDENTITY:--}"
sign_flags=(--force --deep --sign "$app_identity")
if [[ "$app_identity" != "-" ]]; then
    sign_flags+=(--options runtime --timestamp)
fi

mkdir -p "$output" "$work/resources"
cp "$art"/background-light.png "$art"/background-light@2x.png "$art"/background-dark.png "$art"/background-dark@2x.png "$work/resources/"

# component name | source in the artefacts | install location | package id
components=(
    "De-Saturator.vst3|VST3/De-Saturator.vst3|/Library/Audio/Plug-Ins/VST3|com.ariaashrafi.pkg.desaturator.vst3"
    "De-Saturator.component|AU/De-Saturator.component|/Library/Audio/Plug-Ins/Components|com.ariaashrafi.pkg.desaturator.au"
)

refs=""
choices=""
lines=""
for entry in "${components[@]}"; do
    IFS='|' read -r name source location id <<< "$entry"
    payload="$work/payload-$id"
    mkdir -p "$payload"
    # Without extended attributes, which would otherwise travel as stray ._ files in the package.
    ditto --norsrc --noextattr --noacl "$artefacts/$source" "$payload/$name"

    codesign "${sign_flags[@]}" "$payload/$name"
    # Never package a bundle whose signature doesn't match its contents.
    codesign --verify --deep --strict --all-architectures --verbose=2 "$payload/$name"

    # A bundle marked relocatable could install over a copy found elsewhere on the disk.
    pkgbuild --analyze --root "$payload" "$work/component-$id.plist"
    if /usr/libexec/PlistBuddy -c "Print :0:BundleIsRelocatable" "$work/component-$id.plist" >/dev/null 2>&1; then
        /usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" "$work/component-$id.plist"
    fi

    pkgbuild --root "$payload" --component-plist "$work/component-$id.plist" --install-location "$location" \
             --identifier "$id" --version "$version" "$work/$id.pkg"

    refs+="    <pkg-ref id=\"$id\" version=\"$version\" onConclusion=\"none\">$id.pkg</pkg-ref>"$'\n'
    choices+="    <choice id=\"$id\" visible=\"false\"><pkg-ref id=\"$id\"/></choice>"$'\n'
    lines+="            <line choice=\"$id\"/>"$'\n'
done

# Backgrounds are bottom-left aligned with no scaling, as the kit asks.
cat > "$work/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>De-Saturator</title>
    <organization>com.ariaashrafi</organization>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <os-version min="11.0"/>
    <license file="LICENSE.txt"/>
    <background file="background-light.png" alignment="bottomleft" scaling="none" mime-type="image/png"/>
    <background-darkAqua file="background-dark.png" alignment="bottomleft" scaling="none" mime-type="image/png"/>
    <choices-outline>
        <line choice="default">
$lines        </line>
    </choices-outline>
    <choice id="default"/>
$choices$refs</installer-gui-script>
EOF

# The licence the installer shows: the plugin's, then the fonts' (the OFL asks for its text to travel with them).
{
    cat "$root/LICENSE"
    printf '\n\n----------------------------------------------------------------------\nFonts embedded in the plugin: Anybody\n\n'
    cat "$root/design/fonts/Anybody-OFL.txt"
    printf '\n\n----------------------------------------------------------------------\nFonts embedded in the plugin: Departure Mono\n\n'
    cat "$root/design/fonts/DepartureMono-OFL.txt"
} > "$work/resources/LICENSE.txt"

signing=()
if [[ -n "${DESAT_INSTALLER_IDENTITY:-}" ]]; then
    signing=(--sign "$DESAT_INSTALLER_IDENTITY" --timestamp)
fi

package="$output/De-Saturator-$version-macOS.pkg"
productbuild --distribution "$work/distribution.xml" --package-path "$work" --resources "$work/resources" \
    ${signing[@]+"${signing[@]}"} "$package"

if [[ -n "${DESAT_NOTARY_PROFILE:-}" ]]; then
    xcrun notarytool submit "$package" --keychain-profile "$DESAT_NOTARY_PROFILE" --wait
    xcrun stapler staple "$package"
fi

echo "built $package"
