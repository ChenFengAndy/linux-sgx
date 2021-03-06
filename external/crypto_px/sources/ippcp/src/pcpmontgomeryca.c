/*
* Copyright (C) 2016 Intel Corporation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.
*   * Neither the name of Intel Corporation nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include "owndefs.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpmontgomery.h"
#include "pcptool.h"

/*F*
// Name: ippsMontGetSize
//
// Purpose: Specifies size of buffer in bytes.
//
// Returns:                Reason:
//      ippStsNullPtrErr    pCtxSize==NULL
//      ippStsLengthErr     maxLen32 < 1
//                          maxLen32 > BITS2WORD32_SIZE(BN_MAXBITSIZE)
//      ippStsNoErr         no errors
//
// Parameters:
//      method    selected exponential method (unused parameter)
//      maxLen32  max modulus length (in Ipp32u chunks)
//      pCtxSize  size of context
//
// Notes: Function always use method=ippBinaryMethod,
//        so this parameter is ignored
*F*/
IPPFUN(IppStatus, ippsMontGetSize, (IppsExpMethod method, cpSize maxLen32, cpSize* pCtxSize))
{
   IPP_BAD_PTR1_RET(pCtxSize);
   IPP_BADARG_RET(maxLen32<1 || maxLen32>BITS2WORD32_SIZE(BN_MAXBITSIZE), ippStsLengthErr);

   UNREFERENCED_PARAMETER(method);

   {
      /* convert modulus length to the number of BNU_CHUNK_T */
      cpSize modSize = INTERNAL_BNU_LENGTH(maxLen32);

      *pCtxSize= sizeof(IppsMontState)
               + modSize*sizeof(BNU_CHUNK_T)    /* modulus  */
               + modSize*sizeof(BNU_CHUNK_T)    /* identity */
               + modSize*sizeof(BNU_CHUNK_T)    /* square R */
               + modSize*sizeof(BNU_CHUNK_T)    /* cube R */
               + modSize*sizeof(BNU_CHUNK_T)    /* internal buffer */
               + modSize*sizeof(BNU_CHUNK_T)    /* internal sscm buffer */
               + modSize*sizeof(BNU_CHUNK_T)*2  /* internal product */
               + MONT_ALIGNMENT-1;

      return ippStsNoErr;
   }
}


/*F*
// Name: ippsMontInit
//
// Purpose: Initializes the symbolic data structure and partitions the
//      specified buffer space.
//
// Returns:                Reason:
//      ippStsNullPtrErr    pMont==NULL
//      ippStsLengthErr     maxLen32 < 1
//                          maxLen32 > BITS2WORD32_SIZE(BN_MAXBITSIZE)
//      ippStsNoErr         no errors
//
// Parameters:
//      method    selected exponential method (unused parameter)
//      maxLen32  max modulus length (in Ipp32u chunks)
//      pMont     pointer to Montgomery context
*F*/
IPPFUN(IppStatus, ippsMontInit,(IppsExpMethod method, int maxLen32, IppsMontState* pMont))
{
   IPP_BADARG_RET(maxLen32<1 || maxLen32>BITS2WORD32_SIZE(BN_MAXBITSIZE), ippStsLengthErr);

   IPP_BAD_PTR1_RET(pMont);
   pMont = (IppsMontState*)( IPP_ALIGNED_PTR(pMont, MONT_ALIGNMENT) );

   UNREFERENCED_PARAMETER(method);

   MNT_ID(pMont)     = idCtxUnknown;
   MNT_ROOM(pMont)   = INTERNAL_BNU_LENGTH(maxLen32);
   MNT_SIZE(pMont)   = 0;
   MNT_HELPER(pMont) = 0;

   {
      Ipp8u* ptr = (Ipp8u*)pMont;

      /* convert modulus length to the number of BNU_CHUNK_T */
      cpSize modSize = MNT_ROOM(pMont);

      /* assign internal buffers */
      MNT_MODULUS(pMont) = (BNU_CHUNK_T*)( ptr += sizeof(IppsMontState) );

      MNT_1(pMont)       = (BNU_CHUNK_T*)( ptr += modSize*sizeof(BNU_CHUNK_T) );
      MNT_SQUARE_R(pMont)= (BNU_CHUNK_T*)( ptr += modSize*sizeof(BNU_CHUNK_T) );
      MNT_CUBE_R(pMont)  = (BNU_CHUNK_T*)( ptr += modSize*sizeof(BNU_CHUNK_T) );

      MNT_TBUFFER(pMont) = (BNU_CHUNK_T*)( ptr += modSize*sizeof(BNU_CHUNK_T) );
      MNT_SBUFFER(pMont) = (BNU_CHUNK_T*)( ptr += modSize*sizeof(BNU_CHUNK_T) );
      MNT_PRODUCT(pMont) = (BNU_CHUNK_T*)( ptr += modSize*sizeof(BNU_CHUNK_T) );
      MNT_KBUFFER(pMont) = (BNU_CHUNK_T*)NULL;

      /* init internal buffers */
      ZEXPAND_BNU(MNT_MODULUS(pMont), 0, modSize);
      ZEXPAND_BNU(MNT_1(pMont), 0, modSize);
      ZEXPAND_BNU(MNT_SQUARE_R(pMont), 0, modSize);
      ZEXPAND_BNU(MNT_CUBE_R(pMont), 0, modSize);

      MNT_ID(pMont) = idCtxMontgomery;
      return ippStsNoErr;
   }
}


/*F*
// Name: ippsMontSet
//
// Purpose: Setup modulus value
//
// Returns:                   Reason:
//    ippStsNullPtrErr           pMont==NULL
//                               pModulus==NULL
//    ippStsContextMatchErr      !MNT_VALID_ID()
//    ippStsLengthErr            len32<1
//    ippStsNoErr                no errors
//
// Parameters:
//    pModulus    pointer to the modulus buffer
//    len32       length of the  modulus (in Ipp32u chunks).
//    pMont       pointer to the context
*F*/
static BNU_CHUNK_T cpMontHelper(BNU_CHUNK_T m0)
{
   BNU_CHUNK_T y = 1;
   BNU_CHUNK_T x = 2;
   BNU_CHUNK_T mask = 2*x-1;

   int i;
   for(i=2; i<=BNU_CHUNK_BITS; i++, x<<=1) {
      BNU_CHUNK_T rH, rL;
      MUL_AB(rH, rL, m0, y);
      if( x < (rL & mask) ) /* x < ((m0*y) mod (2*x)) */
         y+=x;
      mask += mask + 1;
   }
   return 0-y;
}

IPPFUN(IppStatus, ippsMontSet,(const Ipp32u* pModulus, cpSize len32, IppsMontState* pMont))
{
   IPP_BAD_PTR2_RET(pModulus, pMont);
   pMont = (IppsMontState*)(IPP_ALIGNED_PTR((pMont), MONT_ALIGNMENT));
   IPP_BADARG_RET(!MNT_VALID_ID(pMont), ippStsContextMatchErr);

   IPP_BADARG_RET(len32<1, ippStsLengthErr);

   /* modulus is not an odd number */
   IPP_BADARG_RET((pModulus[0] & 1) == 0, ippStsBadModulusErr);
   IPP_BADARG_RET(MNT_ROOM(pMont)<(int)(INTERNAL_BNU_LENGTH(len32)), ippStsOutOfRangeErr);

   {
      BNU_CHUNK_T m0;
      cpSize len;

      /* fix input modulus */
      FIX_BNU(pModulus, len32);

      /* store modulus */
      ZEXPAND_BNU(MNT_MODULUS(pMont), 0, MNT_ROOM(pMont));
      COPY_BNU((Ipp32u*)(MNT_MODULUS(pMont)), pModulus, len32);
      /* store modulus length */
      len = INTERNAL_BNU_LENGTH(len32);
      MNT_SIZE(pMont) = len;

      /* pre-compute helper m0, m0*m = -1 mod R */
      m0 = cpMontHelper(MNT_MODULUS(pMont)[0]);
      MNT_HELPER(pMont) = m0;

      /* setup identity */
      ZEXPAND_BNU(MNT_1(pMont), 0, len);
      MNT_1(pMont)[len] = 1;
      cpMod_BNU(MNT_1(pMont), len+1, MNT_MODULUS(pMont), len);

      /* setup square */
      ZEXPAND_BNU(MNT_SQUARE_R(pMont), 0, len);
      COPY_BNU(MNT_SQUARE_R(pMont)+len, MNT_1(pMont), len);
      cpMod_BNU(MNT_SQUARE_R(pMont), 2*len, MNT_MODULUS(pMont), len);

      /* setup cube */
      ZEXPAND_BNU(MNT_CUBE_R(pMont), 0, len);
      COPY_BNU(MNT_CUBE_R(pMont)+len, MNT_SQUARE_R(pMont), len);
      cpMod_BNU(MNT_CUBE_R(pMont), 2*len, MNT_MODULUS(pMont), len);

      /* clear buffers */
      ZEXPAND_BNU(MNT_TBUFFER(pMont), 0, len);
      ZEXPAND_BNU(MNT_SBUFFER(pMont), 0, len);
      ZEXPAND_BNU(MNT_PRODUCT(pMont), 0, 2*len);

      return ippStsNoErr;
   }
}


/*F*
// Name: ippsMontMul
//
// Purpose: Computes Montgomery modular multiplication for positive big
//      number integers of Montgomery form. The following pseudocode
//      represents this function:
//      r <- ( a * b * R^(-1) ) mod m
//
// Returns:                Reason:
//      ippStsNoErr         Returns no error.
//      ippStsNullPtrErr    Returns an error when pointers are null.
//      ippStsBadArgErr     Returns an error when a or b is a negative integer.
//      ippStsScaleRangeErr Returns an error when a or b is more than m.
//      ippStsOutOfRangeErr Returns an error when IppsBigNumState *r is larger than
//                          IppsMontState *m.
//      ippStsContextMatchErr Returns an error when the context parameter does
//                          not match the operation.
//
// Parameters:
//      a   Multiplicand within the range [0, m - 1].
//      b   Multiplier within the range [0, m - 1].
//      m   Modulus.
//      r   Montgomery multiplication result.
//
// Notes: The size of IppsBigNumState *r should not be less than the data
//      length of the modulus m.
*F*/
IPPFUN(IppStatus, ippsMontMul, (const IppsBigNumState* pA, const IppsBigNumState* pB, IppsMontState* pMont, IppsBigNumState* pR))
{
   IPP_BAD_PTR4_RET(pA, pB, pMont, pR);

   pMont = (IppsMontState*)(IPP_ALIGNED_PTR((pMont), MONT_ALIGNMENT));
   pA = (IppsBigNumState*)( IPP_ALIGNED_PTR(pA, BN_ALIGNMENT) );
   pB = (IppsBigNumState*)( IPP_ALIGNED_PTR(pB, BN_ALIGNMENT) );
   pR = (IppsBigNumState*)( IPP_ALIGNED_PTR(pR, BN_ALIGNMENT) );

   IPP_BADARG_RET(!MNT_VALID_ID(pMont), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pB), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pR), ippStsContextMatchErr);

   IPP_BADARG_RET(BN_NEGATIVE(pA) || BN_NEGATIVE(pB), ippStsBadArgErr);
   IPP_BADARG_RET(cpCmp_BNU(BN_NUMBER(pA), BN_SIZE(pA), MNT_MODULUS(pMont), MNT_SIZE(pMont)) >= 0, ippStsScaleRangeErr);
   IPP_BADARG_RET(cpCmp_BNU(BN_NUMBER(pB), BN_SIZE(pB), MNT_MODULUS(pMont), MNT_SIZE(pMont)) >= 0, ippStsScaleRangeErr);
   IPP_BADARG_RET(BN_ROOM(pR) < MNT_SIZE(pMont), ippStsOutOfRangeErr);

   {
      BNU_CHUNK_T* pDataR = BN_NUMBER(pR);
      cpSize nsM = MNT_SIZE(pMont);

      cpMontMul_BNU(pDataR,
                    BN_NUMBER(pA), BN_SIZE(pA),
                    BN_NUMBER(pB), BN_SIZE(pB),
                    MNT_MODULUS(pMont), nsM,
                    MNT_HELPER(pMont),
                    MNT_PRODUCT(pMont), MNT_KBUFFER(pMont));

      FIX_BNU(pDataR, nsM);
      BN_SIZE(pR) = nsM;
      BN_SIGN(pR) = ippBigNumPOS;

      return ippStsNoErr;
   }
}
