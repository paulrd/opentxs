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

#ifndef OPENTXS_CORE_OTCHEQUE_HPP
#define OPENTXS_CORE_OTCHEQUE_HPP

#include "OTTrackable.hpp"

namespace opentxs
{

class Cheque : public OTTrackable
{
private: // Private prevents erroneous use by other classes.
    typedef OTTrackable ot_super;

protected:
    virtual int32_t ProcessXMLNode(irr::io::IrrXMLReader*& xml);

    int64_t m_lAmount;
    String m_strMemo;
    Identifier m_RECIPIENT_NYM_ID; // Optional. If present, must match
                                   // depositor's user ID.
    bool m_bHasRecipient;
    Identifier m_REMITTER_NYM_ID; // In the case of vouchers (cashier's
                                  // cheques) we store the Remitter's ID.
    Identifier m_REMITTER_ACCT_ID;
    bool m_bHasRemitter;

public:
    inline void SetAsVoucher(const Identifier& remitterNymID,
                             const Identifier& remitterAcctID)
    {
        m_REMITTER_NYM_ID = remitterNymID;
        m_REMITTER_ACCT_ID = remitterAcctID;
        m_bHasRemitter = true;
        m_strContractType = "VOUCHER";
    }
    inline const String& GetMemo() const
    {
        return m_strMemo;
    }
    inline const int64_t& GetAmount() const
    {
        return m_lAmount;
    }
    inline const Identifier& GetRecipientNymID() const
    {
        return m_RECIPIENT_NYM_ID;
    }
    inline bool HasRecipient() const
    {
        return m_bHasRecipient;
    }
    inline const Identifier& GetRemitterNymID() const
    {
        return m_REMITTER_NYM_ID;
    }
    inline const Identifier& GetRemitterAcctID() const
    {
        return m_REMITTER_ACCT_ID;
    }
    inline bool HasRemitter() const
    {
        return m_bHasRemitter;
    }

    // A cheque HAS NO "Recipient Asset Acct ID", since the recipient's account
    // (where he deposits
    // the cheque) is not known UNTIL the time of the deposit. It's certain not
    // known at the time
    // that the cheque is written...

    // Calling this function is like writing a check...
    EXPORT bool IssueCheque(
        const int64_t& lAmount, const int64_t& lTransactionNum,
        const time64_t& VALID_FROM,
        const time64_t& VALID_TO, // The expiration date (valid from/to dates.)
        const Identifier& SENDER_ACCT_ID, // The asset account the cheque is
                                          // drawn on.
        const Identifier& SENDER_NYM_ID,  // This ID must match the user ID on
                                          // the asset account,
        // AND must verify the cheque signature with that user's key.
        const String& strMemo,                          // Optional memo field.
        const Identifier* pRECIPIENT_NYM_ID = nullptr); // Recipient
                                                        // optional. (Might
                                                        // be a blank
                                                        // cheque.)

    EXPORT void CancelCheque(); // You still need to re-sign the cheque after
                                // doing this.

    // From OTTrackable (parent class of this)
    /*
     // A cheque can be written offline, provided you have a transaction
     // number handy to write it with. (Necessary to prevent double-spending.)
     inline       int64_t              GetTransactionNum() const  { return
     m_lTransactionNum; }
     inline const OTIdentifier&    GetSenderAcctID()           { return
     m_SENDER_ACCT_ID; }
     inline const OTIdentifier&    GetSenderNymID()           { return
     m_SENDER_NYM_ID; }
     */

    // From OTInstrument (parent class of OTTrackable, parent class of this)
    /*
     OTInstrument(const OTIdentifier& NOTARY_ID, const OTIdentifier&
     INSTRUMENT_DEFINITION_ID)
     : Contract()

     inline const OTIdentifier& GetInstrumentDefinitionID()  const { return
     m_InstrumentDefinitionID; }
     inline const OTIdentifier& GetNotaryID() const { return m_NotaryID;    }

     inline time64_t GetValidFrom()    const { return m_VALID_FROM; }
     inline time64_t GetValidTo()        const { return m_VALID_TO;   }

     bool VerifyCurrentDate(); // Verify the current date against the VALID FROM
     / TO dates.
     */
    EXPORT Cheque();
    EXPORT Cheque(const Identifier& NOTARY_ID,
                  const Identifier& INSTRUMENT_DEFINITION_ID);
    EXPORT virtual ~Cheque();

    void InitCheque();
    virtual void Release();
    void Release_Cheque();
    virtual void UpdateContents(); // Before transmission or serialization, this
                                   // is where the token saves its contents
};

} // namespace opentxs

#endif // OPENTXS_CORE_OTCHEQUE_HPP
