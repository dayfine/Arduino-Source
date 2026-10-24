# Keep local privacy grants tied to a persistent certificate rather than a binary hash.
# All deployment commands must precede this signing step.
function(pa_sign_local_macos_app target)
    if (PA_MACOS_SIGNING_IDENTITY STREQUAL "-")
        message(FATAL_ERROR "Local signing requires a persistent certificate, not an ad-hoc signature.")
    endif()
    find_program(PA_CODESIGN_EXECUTABLE codesign REQUIRED)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${PA_CODESIGN_EXECUTABLE}" --force --timestamp=none
            --sign "${PA_MACOS_SIGNING_IDENTITY}"
            --identifier "${PA_MACOS_BUNDLE_IDENTIFIER}"
            "$<TARGET_BUNDLE_DIR:${target}>"
        COMMAND "${PA_CODESIGN_EXECUTABLE}" --verify --strict --verbose=2
            "$<TARGET_BUNDLE_DIR:${target}>"
        COMMENT "Signing local macOS app with persistent identity"
        VERBATIM
    )
endfunction()
