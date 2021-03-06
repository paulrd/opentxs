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

#include <opentxs/core/crypto/CryptoAsymmetric.hpp>

#include <opentxs/core/OTData.hpp>
#include <opentxs/core/crypto/OTSignature.hpp>

namespace opentxs
{

bool CryptoAsymmetric::SignContract(
    const String& strContractUnsigned,
    const OTAsymmetricKey& theKey,
    OTSignature& theSignature, // output
    const CryptoHash::HashType hashType,
    const OTPasswordData* pPWData) const
{
    OTData plaintext(strContractUnsigned.Get(), strContractUnsigned.GetLength()+1); //include null terminator
    OTData signature;

    bool success = Sign(
                        plaintext,
                        theKey,
                        hashType,
                        signature,
                        pPWData);

    theSignature.SetData(signature, true); // true means, "yes, with newlines
                                              // in the b64-encoded output,
                                              // please."
    return success;
}

bool CryptoAsymmetric::VerifyContractSignature(
    const String& strContractToVerify,
    const OTAsymmetricKey& theKey,
    const OTSignature& theSignature,
    const CryptoHash::HashType hashType,
    const OTPasswordData* pPWData) const
{
    OTData plaintext(strContractToVerify.Get(), strContractToVerify.GetLength()+1); //include null terminator
    OTData signature;
    theSignature.GetData(signature);

    return Verify(
            plaintext,
            theKey,
            signature,
            hashType,
            pPWData);

}

} // namespace opentxs
