/**
 * @file cmdFormat.h
 *
 * This API is used to process IOT Key Store formatted wrapping keys and build authenticated
 * command packages.  This code can be included in an implementation of an authenticated server.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef IOT_KEY_STORE_AUTH_SERVER_CMD_FORMAT_INCLUDE_GUARD
#define IOT_KEY_STORE_AUTH_SERVER_CMD_FORMAT_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Creates a wrapping key package.
 *
 * The key value must be a public key in one of the following formats.
 * For RSA the key value is in PKCS #1 format (DER encoded).
 * For ECIES the key value is in ECPoint format defined in RFC5480.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if keyValPtr, keyPackagePtr or keyPackageSizePtr is NULL.
 *      IKS_INVALID_KEY if keyType is not supported.
 *      IKS_OVERFLOW if the keyPackagePtr buffer is too small to hold the key value.
 *      IKS_INTERNAL_ERROR if there was an internal error.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t cmd_CreateWrappingKeyPackage
(
    iks_KeyType_t       keyType,                ///< [IN] Key type.
    size_t              keySize,                ///< [IN] Key size in bytes.
    const uint8_t*      keyValPtr,              ///< [IN] Key value.
    size_t              keyValSize,             ///< [IN] Key value size.
    uint8_t*            keyPackagePtr,          ///< [OUT] Key package buffer.
    size_t*             keyPackageSizePtr       ///< [IN/OUT] On entry: key package buffer size.
                                                ///           On exit: key package size.
);


//--------------------------------------------------------------------------------------------------
/**
 * Reads a wrapping key package to get its contents.
 *
 * For RSA the key value is in PKCS #1 format (DER encoded).
 * For ECIES the key value is in ECPoint format defined in RFC5480.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if keyPackagePtr, keyTypePtr, keySizePtr, keyValPtr or keyValSizePtr is
 *                        NULL.
 *      IKS_OUT_OF_RANGE if keyPackageSize is zero.
 *      IKS_FORMAT_ERROR if the key package is malformed.
 *      IKS_OVERFLOW if the keyValPtr buffer is too small to hold the key value.
 *      IKS_INTERNAL_ERROR if there was an internal error.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t cmd_ReadWrappingKeyPackage
(
    const uint8_t*      keyPackagePtr,          ///< [IN] Key package.
    size_t              keyPackageSize,         ///< [IN] Key package size.
    iks_KeyType_t*      keyTypePtr,             ///< [OUT] Key type.
    size_t*             keySizePtr,             ///< [OUT] Key size in bytes.
    uint8_t*            keyValPtr,              ///< [OUT] Key value buffer.
    size_t*             keyValSizePtr           ///< [IN/OUT] On entry: key value buffer size.
                                                ///           On exit: key value size.
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a raw key management command that can be signed using the associated update key.
 *
 * The wrapKeyFpPtr refers to the fingerprint of the key that will be used to wrap the authenticated
 * command.  The authenticated command only requires wrapping if the provisioning data contains a
 * symmetric key.  The fingerprint must be calculate using the IKS_FINGERPRINT_FUNC.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if challengePtr, targetIdPtr, provDataPtr, wrapKeyFpPtr, rawCmdPtr or
 *                        rawCmdSizePtr is NULL when it shouldn't be.
 *      IKS_OUT_OF_RANGE if the version number or challenge size is unsupported.
 *      IKS_OVERFLOW if the rawCmdPtr buffer is too small to hold the raw command value.
 *      IKS_INTERNAL_ERROR if there was an internal error.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t cmd_CreateRawCmd
(
    uint16_t            version,                ///< [IN] Format version.
    iks_Cmd_t           cmd,                    ///< [IN] Command to create.
    const uint8_t*      challengePtr,           ///< [IN] Challenge for the command.
    size_t              challengeSize,          ///< [IN] Challenge size.
    const char*         targetIdPtr,            ///< [IN] ID of the target key/digest.
    const uint8_t*      provDataPtr,            ///< [IN] Provisioning data.  NULL for no data.
    size_t              provDataSize,           ///< [IN] Provisioning data size.
    const uint8_t*      wrapKeyFpPtr,           ///< [IN] Wrapping key's fingerprint.  NULL if not
                                                ///       used.
    size_t              wrapKeyFpSize,          ///< [IN] Wrapping key's fingerprint size.
    uint8_t*            rawCmdPtr,              ///< [OUT] Raw command buffer.
    size_t*             rawCmdSizePtr           ///< [IN/OUT] On entry: raw command buffer size.
                                                ///           On exit: raw command size.
);


//--------------------------------------------------------------------------------------------------
/**
 * Read a raw key management command package.
 *
 * The wrappingKeyPtr refers to the key that was used to wrap the authenticated command.
 *
 * The command is considered malformed if provisioning data and/or a fingerprint is expected but not
 * found or when they are not expected but are present.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if rawCmdPtr, challengePtr, challengeSizePtr, targetIdPtr is NULL.
 *      IKS_OUT_OF_RANGE if rawCmdSize is unexpected.
 *      IKS_FORMAT_ERROR if the raw command package is malformed.
 *      IKS_OVERFLOW if any of the output buffers is too small to hold the result.
 *      IKS_INTERNAL_ERROR if there was an internal error.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t cmd_ReadRawCmd
(
    const uint8_t*      rawCmdPtr,              ///< [IN] Raw command package.
    size_t              rawCmdSize,             ///< [IN] Raw command package size.
    iks_Cmd_t           expectedCmdType,        ///< [IN] Command type.
    uint8_t*            challengePtr,           ///< [OUT] Challenge buffer.
    size_t*             challengeSizePtr,       ///< [IN/OUT] On entry: Challenge buffer size.
                                                ///           On exit: Challenge size.
    char*               targetIdPtr,            ///< [OUT] Target ID buffer.
    size_t              targetIdSize,           ///< [IN] Target ID buffer size.
    uint8_t*            provDataPtr,            ///< [OUT] Provisioning data buffer.  Should be NULL
                                                ///        if no provisioning data is expected.
    size_t*             provDataSizePtr,        ///< [IN/OUT] On entry: Provisioning data buffer size.
                                                ///           On exit: Provisioning data size.
    uint8_t*            wrapKeyFpPtr,           ///< [OUT] Wrapping key's fingerprint.  NULL if not
                                                ///        no fingerprint is expected.
    size_t*             wrapKeyFpSizePtr        ///< [IN/OUT] On entry: Fingerprint buffer size.
                                                ///           On exit: Fingerprint size.
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates an authenticated command.
 *
 * If the signing key is an RSA key then the param value is interpreted as the salt length.
 * If the signing key is an ECDSA key then the param value is interpreted as the hash function
 * (iks_HashFunc_t) used to create the digest of the raw command.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if rawCmdPtr, sigPtr, authCmdPtr or authCmdSizePtr is NULL.
 *      IKS_OUT_OF_RANGE if rawCmdSize or sigSize is zero.
 *      IKS_OVERFLOW if the authCmdPtr buffer is too small to hold the authenticated command value.
 *      IKS_INTERNAL_ERROR if there was an internal error.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t cmd_CreateAuthCmd
(
    const uint8_t*      rawCmdPtr,              ///< [IN] Raw command.
    size_t              rawCmdSize,             ///< [IN] Raw command size.
    const uint8_t*      sigPtr,                 ///< [IN] Signature of the raw command.
    size_t              sigSize,                ///< [IN] Signature size.
    size_t              param,                  ///< [IN] Signature parameter; see comment above.
    uint8_t*            authCmdPtr,             ///< [OUT] Authenticated command buffer.
    size_t*             authCmdSizePtr          ///< [IN/OUT] On entry: Auth command buffer size.
                                                ///           On exit: Auth command size.
);


//--------------------------------------------------------------------------------------------------
/**
 * Read an authenticated command.
 *
 * If the signing key is an RSA key then the param value is interpreted as the salt length.
 * If the signing key is an ECDSA key then the param value is interpreted as the hash function
 * (iks_HashFunc_t) used to create the digest of the raw command.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if authCmdPtr, sigPtr, sigSizePtr, paramPtr, rawCmdPtr or rawCmdSizePtr is
 *                        NULL.
 *      IKS_OUT_OF_RANGE if authCmdSize is unexpected.
 *      IKS_FORMAT_ERROR if the authenticated command is malformed.
 *      IKS_OVERFLOW if the rawCmdPtr buffer is too small to hold the raw command value.
 *      IKS_INTERNAL_ERROR if there was an internal error.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t cmd_ReadAuthCmd
(
    const uint8_t*      authCmdPtr,             ///< [IN] Authenticated command.
    size_t              authCmdSize,            ///< [IN] Authenticated command size.
    uint8_t*            sigPtr,                 ///< [OUT] Signature buffer.
    size_t*             sigSizePtr,             ///< [IN/OUT] On entry: Signature buffer size.
                                                ///           On exit: Signature size.
    size_t*             paramPtr,               ///< [OUT] Signature parameter; see comment above.
    uint8_t*            rawCmdPtr,              ///< [OUT] Raw command buffer.
    size_t*             rawCmdSizePtr           ///< [IN/OUT] On entry: Raw command buffer size.
                                                ///           On exit: Raw command size.
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a wrapped data package.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if ephemKeyPtr, saltPtr, tagPtr, ciphertextPtr, wrappedPtr or
 *                        wrappedSizePtr is NULL.
 *      IKS_OUT_OF_RANGE if ephemKeySize, tagSize or ciphertextSize is zero.
 *      IKS_OVERFLOW if the wrappedPtr buffer is too small to hold the wrapped key.
 *      IKS_INTERNAL_ERROR if there was an internal error.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t cmd_CreateWrappedData
(
    const uint8_t*      ephemKeyPtr,            ///< [IN] Serialized ephemeral or encrypted bulk key.
    size_t              ephemKeySize,           ///< [IN] Serialized ephemeral key size.
    const uint8_t*      tagPtr,                 ///< [IN] Authentication tag.
    size_t              tagSize,                ///< [IN] Authentication tag size.
    const uint8_t*      ciphertextPtr,          ///< [IN] Encrypted key.
    size_t              ciphertextSize,         ///< [IN] Encrypted key size.
    uint8_t*            wrappedPtr,             ///< [OUT] Wrapped key buffer.
    size_t*             wrappedSizePtr          ///< [IN/OUT] On entry: Wrapped key buffer size.
                                                ///           On exit: Wrapped key size.
);


//--------------------------------------------------------------------------------------------------
/**
 * Reads a wrapped data package.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if packagePtr, ephemKeyPtr, ephemKeySizePtr, saltPtr, saltSizePtr,
 *                        tagPtr, tagSizePtr, ciphertextPtr or ciphertextSizePtr is NULL.
 *      IKS_OUT_OF_RANGE if packageSize is unexpected.
 *      IKS_FORMAT_ERROR if the package is malformed.
 *      IKS_OVERFLOW if any of the buffers are too small to hold the result.
 *      IKS_INTERNAL_ERROR if there was an internal error.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t cmd_ReadWrappedData
(
    const uint8_t*      packagePtr,             ///< [IN] Package to read.
    size_t              packageSize,            ///< [IN] Package size.
    uint8_t*            ephemKeyPtr,            ///< [OUT] Ephemeral or encrypted bulk key buffer.
    size_t*             ephemKeySizePtr,        ///< [IN/OUT] On entry: Ephemeral key buffer size.
                                                ///           On exit: Ephemeral key size.
    uint8_t*            tagPtr,                 ///< [OUT] Auth tag buffer.
    size_t*             tagSizePtr,             ///< [IN/OUT] On entry: Auth tag buffer size.
                                                ///           On exit: Auth tag size.
    uint8_t*            ciphertextPtr,          ///< [OUT] Encrypted key buffer.
    size_t*             ciphertextSizePtr       ///< [IN/OUT] On entry: Encrypted key buffer size.
                                                ///           On exit: Encrypted key size.
);


#endif  // IOT_KEY_STORE_AUTH_SERVER_CMD_FORMAT_INCLUDE_GUARD
