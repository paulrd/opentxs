/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#include <opentxs/core/crypto/Letter.hpp>

#include <opentxs/core/Log.hpp>
#include <opentxs/core/Nym.hpp>
#include <opentxs/core/app/App.hpp>
#include <opentxs/core/crypto/NymParameters.hpp>
#include <opentxs/core/crypto/OTAsymmetricKey.hpp>
#include <opentxs/core/crypto/OTEnvelope.hpp>
#include <opentxs/core/crypto/OTKeypair.hpp>
#include <opentxs/core/util/Tag.hpp>

#if defined(OT_CRYPTO_USING_LIBSECP256K1)
#include <opentxs/core/crypto/Libsecp256k1.hpp>
#endif

#include <cstring>
#include <irrxml/irrXML.hpp>
#include <opentxs/core/crypto/OpenSSL.hpp>

namespace opentxs
{

void Letter::UpdateContents()
{
    // I release this because I'm about to repopulate it.
    m_xmlUnsigned.Release();

    Tag rootNode("letter");

    rootNode.add_attribute("ephemeralkey", ephemeralKey_.Get());
    rootNode.add_attribute("iv", iv_.Get());
    rootNode.add_attribute("tag", tag_.Get());
    rootNode.add_attribute("mode", plaintextMode_.Get());

    for (auto& it : sessionKeys_) {
        TagPtr sessionKeyNode = std::make_shared<Tag>("sessionkey");
        OTASCIIArmor sessionKey;

        std::get<4>(it)->GetAsciiArmoredData(sessionKey);

        sessionKeyNode->add_attribute("algo", std::get<0>(it).Get());
        sessionKeyNode->add_attribute("hmac", std::get<1>(it).Get());
        sessionKeyNode->add_attribute("nonce", std::get<2>(it).Get());
        sessionKeyNode->add_attribute("tag", std::get<3>(it).Get());
        sessionKeyNode->set_text(sessionKey.Get());

        rootNode.add_tag(sessionKeyNode);
    }

    rootNode.add_tag("ciphertext", ciphertext_.Get());

    std::string str_result;
    rootNode.output(str_result);

    m_xmlUnsigned.Concatenate("%s", str_result.c_str());
}

int32_t Letter::ProcessXMLNode(irr::io::IrrXMLReader*& xml)
{
    int32_t nReturnVal = 0;

    const String strNodeName(xml->getNodeName());

    if (strNodeName.Compare("letter")) {
        ephemeralKey_ = xml->getAttributeValue("ephemeralkey");
        iv_ = xml->getAttributeValue("iv");
        tag_ = xml->getAttributeValue("tag");
        plaintextMode_ = xml->getAttributeValue("mode");
        nReturnVal = 1;
    } else if (strNodeName.Compare("ciphertext")) {
        if (false ==
            Contract::LoadEncodedTextField(xml, ciphertext_)) {
            otErr << "Error in Letter::ProcessXMLNode: no ciphertext.\n";
            return (-1); // error condition
        }
        nReturnVal = 1;
    } else if (strNodeName.Compare("sessionkey")) {
        String algo, hmac, nonce, tag;
        OTASCIIArmor armoredText;

        algo = xml->getAttributeValue("algo");
        hmac = xml->getAttributeValue("hmac");
        nonce = xml->getAttributeValue("nonce");
        tag = xml->getAttributeValue("tag");

        if (false == Contract::LoadEncodedTextField(xml, armoredText)) {
            otErr << "Error in Letter::ProcessXMLNode: no ciphertext.\n";

            return (-1); // error condition
        } else {
            sessionKeys_.push_back(symmetricEnvelope(algo, hmac, nonce, tag, std::make_shared<OTEnvelope>(armoredText)));

            nReturnVal = 1;
        }
    }

    return nReturnVal;
}

Letter::Letter(
    const String& ephemeralKey,
    const String& iv,
    const String& tag,
    const String& mode,
    const OTASCIIArmor& ciphertext,
    const listOfSessionKeys& sessionKeys)
        : Contract()
        , ephemeralKey_(ephemeralKey)
        , iv_(iv)
        , tag_(tag)
        , plaintextMode_(mode)
        , ciphertext_(ciphertext)
        , sessionKeys_(sessionKeys)

{
    m_strContractType.Set("LETTER");
}

Letter::Letter(
        const String& input)
    : Contract()
{
    m_strContractType.Set("LETTER");
    m_xmlUnsigned = input;
    LoadContractXML();
}

Letter::~Letter()
{
    Release_Letter();
}

void Letter::Release_Letter()
{
}

void Letter::Release()
{
    Release_Letter();

    ot_super::Release();

    m_strContractType.Set("LETTER");
}

bool Letter::Seal(
    const mapOfAsymmetricKeys& RecipPubKeys,
    const String& theInput,
    OTData& dataOutput)
{
    bool haveRecipientsECDSA = false;
    bool haveRecipientsRSA = false;
    mapOfAsymmetricKeys secp256k1Recipients;
    mapOfAsymmetricKeys RSARecipients;

    for (auto& it : RecipPubKeys) {
        switch (it.second->keyType()) {
            case OTAsymmetricKey::SECP256K1 :
                haveRecipientsECDSA = true;
                secp256k1Recipients.insert(std::pair<std::string, OTAsymmetricKey*>(it.first, it.second));
                break;
            case OTAsymmetricKey::LEGACY :
                haveRecipientsRSA = true;
                RSARecipients.insert(std::pair<std::string, OTAsymmetricKey*>(it.first, it.second));
                break;
            default :
                otErr << "Letter::" << __FUNCTION__ << ": Unknown recipient type.\n";
                return false;
        }
    }

    // The plaintext will be encrypted to this symmetric key.
    // The session key will be individually encrypted to every recipient.
    BinarySecret masterSessionKey = App::Me().Crypto().AES().InstantiateBinarySecretSP();
    masterSessionKey->randomizeMemory(CryptoSymmetric::KeySize(defaultPlaintextMode_));

    // Obtain an iv in both binary form, and base58check String form.
    OTData iv;
    String ivReadable = App::Me().Crypto().Util().Nonce(CryptoSymmetric::IVSize(defaultPlaintextMode_), iv);
    OTData tag;

    // Now that we have a session key, encrypt the plaintext
    OTData ciphertext;
    bool encrypted = App::Me().Crypto().AES().Encrypt(
        defaultPlaintextMode_,
        *masterSessionKey,
        iv,
        theInput.Get(),
        theInput.GetLength(),
        ciphertext,
        tag);

    if (encrypted) {
        OTASCIIArmor encodedCiphertext(ciphertext);
        String ephemeralPubkey;
        listOfSessionKeys sessionKeys;
        String macType = "null";

        String tagReadable = CryptoUtil::Base58CheckEncode(tag);

        if (haveRecipientsECDSA) {
            #if defined(OT_CRYPTO_USING_LIBSECP256K1)
            Libsecp256k1& engine = static_cast<Libsecp256k1&>(App::Me().Crypto().SECP256K1());
            macType = CryptoHash::HashTypeToString(Libsecp256k1::ECDHDefaultHMAC);

            // Generate an ephemeral keypair for ECDH shared secret derivation.
            // Why not use the sender's secp256k1 key for this?
            // Because maybe the sender only has RSA credentials.
            NymParameters pKeyData(
                NymParameters::SECP256K1,
                proto::CREDTYPE_LEGACY);

            OTKeypair ephemeralKeypair(pKeyData);
            ephemeralKeypair.GetPublicKey(ephemeralPubkey);

            const OTAsymmetricKey& ephemeralPrivkey = ephemeralKeypair.GetPrivateKey();

            // Individually encrypt the session key to each recipient and add the encrypted key
            // to the global list of session keys for this letter.
            for (auto it : secp256k1Recipients) {
                symmetricEnvelope encryptedSessionKey(
                    CryptoSymmetric::ModeToString(defaultSessionKeyMode_),
                    CryptoHash::HashTypeToString(defaultHMAC_),
                    "",
                    "",
                    nullptr);

                OTPasswordData passwordData("ephemeral");
                bool haveSessionKey = engine.EncryptSessionKeyECDH(
                                        *masterSessionKey,
                                        ephemeralPrivkey,
                                        *(it.second),
                                        passwordData,
                                        encryptedSessionKey);
                if (haveSessionKey) {
                    sessionKeys.push_back(encryptedSessionKey);

                } else {
                    otErr << "Letter::" << __FUNCTION__ << ": Session key encryption failed.\n";
                    return false;
                }
            }
            #else

            otErr << "Letter::" << __FUNCTION__ << ": Attempting to Seal to secp256k1 recipients"
                <<" without Libsecp256k1 support.\n";
            return false;
            #endif
        }

        if (haveRecipientsRSA) {
            #if defined(OT_CRYPTO_USING_OPENSSL)
            OpenSSL& engine = static_cast<OpenSSL&>(App::Me().Crypto().RSA());

            // Encrypt the session key to all RSA recipients and add the encrypted key
            // to the global list of session keys for this letter.
            OTData ciphertext;

            bool haveSessionKey = engine.EncryptSessionKey(RSARecipients, *masterSessionKey, ciphertext);

            if (haveSessionKey) {
                symmetricEnvelope sessionKeyItem("", "", "", "", std::make_shared<OTEnvelope>(ciphertext));
                sessionKeys.push_back(sessionKeyItem);

            } else {
                otErr << "Letter::" << __FUNCTION__ << ": Session key encryption failed.\n";
                return false;
            }
            #else

            otErr << "Letter::" << __FUNCTION__ << ": Attempting to Seal to OpenSSL recipients"
                <<" without OpenSSL support.\n";
            return false;
            #endif
        }

        // Construct the Letter
        Letter theLetter(
            ephemeralPubkey,
            ivReadable,
            tagReadable,
            CryptoSymmetric::ModeToString(defaultPlaintextMode_),
            encodedCiphertext,
            sessionKeys
        );

        // Serialize the Letter to a String
        String output;
        theLetter.UpdateContents();
        theLetter.SaveContents(output);

        //Encode the serialized Letter into OTData and set the output
        OTASCIIArmor armoredOutput(output);
        OTData finishedOutput(armoredOutput);
        dataOutput.Assign(finishedOutput);

        return true;
    } else {
            otErr << "Letter::" << __FUNCTION__ << ": Encryption failed.\n";
            return false;
    }
    return false;
}

bool Letter::Open(
    const OTData& dataInput,
    const Nym& theRecipient,
    String& theOutput,
    const OTPasswordData* pPWData)
{
    OTASCIIArmor armoredInput(dataInput);
    String decodedInput;
    OTData decodedCiphertext;
    OTData passwordHash;
    BinarySecret sessionKey  = App::Me().Crypto().AES().InstantiateBinarySecretSP();;
    OTData plaintext;

    bool haveDecodedInput = armoredInput.GetString(decodedInput);

    if (!haveDecodedInput) {
        otErr << "Letter::" << __FUNCTION__ << " Could not decode armored input data.\n";
        return false;
    }

    Letter contents(decodedInput);
    OTASCIIArmor ciphertext = contents.Ciphertext();

    if (!ciphertext.Exists()) {
            otErr << "Letter::" << __FUNCTION__ << " Could not retrieve the encoded ciphertext from the Letter.\n";
            return false;
    }

    // Extract and decode the nonce
    OTData iv;
    bool ivDecoded = CryptoUtil::Base58CheckDecode(contents.IV().Get(), iv);

    if (!ivDecoded) {
            otErr << "Letter::" << __FUNCTION__ << " Could not retrieve the iv from the Letter.\n";
            return false;
    }

    // Extract and decode the AEAD tag
    OTData tag;
    bool tagDecoded = CryptoUtil::Base58CheckDecode(contents.AEADTag().Get(), tag);

    if (!tagDecoded) {
        otErr << "Letter::" << __FUNCTION__ << " Could not retrieve the tag from the Letter.\n";
        return false;
    }

    // Extract and decode the plaintext encryption mode
    CryptoSymmetric::Mode mode = contents.Mode();

    if (CryptoSymmetric::ERROR_MODE == mode) {
        otErr << "Letter::" << __FUNCTION__ << " Unsupported or missing plaintext encryption mode.\n";
        return false;
    }

    // Extract and decode the ciphertext
    bool haveDecodedCiphertext = ciphertext.GetData(decodedCiphertext);

    if (!haveDecodedCiphertext) {
        otErr << "Letter::" << __FUNCTION__ << " Could not decode armored ciphertext.\n";
        return false;
    }

    // Attempt to decrypt the session key
    bool haveSessionKey = false;
    const OTAsymmetricKey& privateKey = theRecipient.GetPrivateEncrKey();

    if (privateKey.keyType() == OTAsymmetricKey::SECP256K1) {
        // Decode ephemeral public key
        String ephemeralPubkey(contents.EphemeralKey());

        if(ephemeralPubkey.Exists()) {
            OTAsymmetricKey* publicKey = OTAsymmetricKey::KeyFactory(OTAsymmetricKey::SECP256K1, ephemeralPubkey);

            OT_ASSERT(nullptr != publicKey);

            // Get all the session keys
            listOfSessionKeys sessionKeys(contents.SessionKeys());

            Libsecp256k1& engine = static_cast<Libsecp256k1&>(privateKey.engine());

            // The only way to know which session key (might) belong to us to try them all
            for (auto& it : sessionKeys) {

                if (nullptr == pPWData) {
                    OTPasswordData passwordData("Letter::Open(): Please enter your password to decrypt this document.");
                    haveSessionKey = engine.DecryptSessionKeyECDH(
                        it,
                        privateKey,
                        *publicKey,
                        passwordData,
                        *sessionKey);
                } else {
                    haveSessionKey = engine.DecryptSessionKeyECDH(
                        it,
                        privateKey,
                        *publicKey,
                        *pPWData,
                        *sessionKey);
                }

                if (haveSessionKey) {
                    break;
                }
            }
            // We're done with this
            if (nullptr != publicKey) {
                delete publicKey;
                publicKey = nullptr;
            }
        } else {
            otErr << "Letter::" << __FUNCTION__ << " Need an ephemeral public key for ECDH, but "
                << "the letter does not contain one.\n";
            return false;
        }
    }

    if (privateKey.keyType() == OTAsymmetricKey::LEGACY) {

        // Get all the session keys
        listOfSessionKeys sessionKeys(contents.SessionKeys());

        OpenSSL& engine = static_cast<OpenSSL&>(App::Me().Crypto().RSA());

        // The only way to know which session key (might) belong to us to try them all
        for (auto& it : sessionKeys) {
            haveSessionKey = engine.DecryptSessionKey(std::get<4>(it)->m_dataContents, theRecipient, *sessionKey, pPWData);

            if (haveSessionKey) {
                break;
            }
        }
    }

    if (haveSessionKey) {
        bool decrypted = App::Me().Crypto().AES().Decrypt(
                                                            mode,
                                                            *sessionKey,
                                                            iv,
                                                            tag,
                                                            static_cast<const char*>(decodedCiphertext.GetPointer()),
                                                            decodedCiphertext.GetSize(),
                                                            plaintext);

        if (decrypted) {
            theOutput.Set(static_cast<const char*>(plaintext.GetPointer()), plaintext.GetSize());
            return true;
        } else {
            otErr << "Letter::" << __FUNCTION__ << " Decryption failed.\n";
            return false;
        }
    } else {
        otErr << "Letter::" << __FUNCTION__ << " Could not decrypt any sessions key. "
            << "Was this message intended for us?\n";
        return false;
    }
}

const String& Letter::EphemeralKey() const
{
    return ephemeralKey_;
}

const String& Letter::IV() const
{
    return iv_;
}

const String& Letter::AEADTag() const
{
    return tag_;
}

CryptoSymmetric::Mode Letter::Mode() const
{
    return CryptoSymmetric::StringToMode(plaintextMode_);
}

const listOfSessionKeys& Letter::SessionKeys() const
{
    return sessionKeys_;
}

const OTASCIIArmor& Letter::Ciphertext() const
{
   return ciphertext_;
}

} // namespace opentxs
