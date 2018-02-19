//--------------------------------------------------------------------------------------------------
/**
 * Simple test app that creates and pushes asset data
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

static int TestCase = 0;

// Push callback handler
static void PushCallbackHandler
(
    le_avdata_PushStatus_t status,
    void* contextPtr
)
{
    int value = (int)contextPtr;
    LE_INFO("PushCallbackHandler: %d, value: %d", status, value);

    if ((status == LE_AVDATA_PUSH_SUCCESS) && (value == TestCase))
    {
        LE_INFO("Push test case %d successful", TestCase);
        exit(EXIT_SUCCESS);
    }
    else
    {
        LE_ERROR("Push test case %d failed", TestCase);
        exit(EXIT_FAILURE);
    }
}


// pushing an asset that is not created
void PushNonExistentAsset()
{
    LE_ASSERT(le_avdata_Push("/asdf/zxcv", PushCallbackHandler, NULL) == LE_NOT_FOUND);
    PushCallbackHandler(LE_AVDATA_PUSH_SUCCESS, (void *)1);
}


// pushing an asset that is not created
void PushNotValidAsset()
{
    LE_ASSERT(le_avdata_Push("/asdf////", PushCallbackHandler, NULL) == LE_FAULT);
    PushCallbackHandler(LE_AVDATA_PUSH_SUCCESS, (void *)2);
}

// pushing single element
void PushSingle()
{
    LE_ASSERT(le_avdata_CreateResource("/asset/value", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt("/asset/value", 5) == LE_OK);
    LE_ASSERT(le_avdata_Push("/asset/value", PushCallbackHandler, (void *)3) == LE_OK);
}

// pushing multiple element
void PushMulti()
{
    LE_ASSERT(le_avdata_CreateResource("/asset/value1", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("/asset/value2", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("/asset/value3", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("/asset/value4", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);

    LE_ASSERT(le_avdata_SetInt("/asset/value1", 7) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat("/asset/value2", 1.23) == LE_OK);
    LE_ASSERT(le_avdata_SetString("/asset/value3", "slashDelimiter") == LE_OK);
    LE_ASSERT(le_avdata_SetBool("/asset/value4", false) == LE_OK);
    LE_ASSERT(le_avdata_Push("/asset", PushCallbackHandler, (void *)4) == LE_OK);
}

// pushing large chunk of data
void PushData()
{
    LE_INFO("Push file");
    char largeString[] = "z3vD3so8GXj35nWwWblPBVGoaAxLoNIB3UXmIpe6U57NfVN2BZFxExMPJrHdvIs15q4MnpKAMAiIuymIAoUuYTqqvCnUDzotyxuLpNVC0GsoIevLrpwa0KWVMEAqd5UEFvpdn"
                         "w12DH4ILIMHPWlZygp9qAvr8tQsIkoKBy6xC2sCcBdPUn5gnu7KY0dCIzZT3jPdPI3WTiF06FRfHs2rDJnfoHpzbPOTqvVxbaaVZbvbbQ7zLG3IsNHwSCOCkQJC2V8D2pN0fc"
                         "mG86q1102Kl9rOBZ7eiKMVGIsdlTZRaCkfnCUqIrnQFytszMUdSqmeATa8vGw4vieMpGOXK0nFrJQbXbGj4v83k6UX8MlxcsEue9ySxfvyVftQV9EEkxGRjZ4CXrGVjXXoACj"
                         "x0WUBaBZFcXy5AfgN1jUMMHaDSicKpeVdfF8R9U15Q3iVmiVCy8AhEmfhBrSb4Qiu43JCI6zzbKBzemeC2WYX5IAVZcIhog3z9gqNbbo66ExDgkQKtXDNiGPwldERKRJ83Pt5"
                         "3zqgjUKaJIEVnCCpspHXkw6jjKvBYBDqdoNwkyiAhfeiEAok6Bib7FH9RUgWmT1utHalFY8Zrl6jxd0yCIch2LMPfUDlgU1TJM4jzB2U23YRnaw2lRqAr5KDYBCztIQXP8se0"
                         "ShZUmsPbgwxiAGhIyahwcNnLatXcHv6aUWxS94C2cx160TnW26prpcZ8afnhVuXP1GsLQUHqPyfE9t0V2YruBYdcUpDSizvLecZxOUninUmGsdKzZy1hcpn5LtkUZrIFkcATl"
                         "m1TTfbiXzXKAeWCKr6IriL55JfgLw5KleF3fCGseuEHktMKdNmmuCvsNr2JbwKPkbYqMZJ6hAqM2ovTHLt9YJmZrf0AcTMSLRAPDuKZERQcKsP0OsD6Sun4wXpLZ1Ra7OgKT6"
                         "dRL4ie339fuqGyJoz8K7TntJXjbOMN2uL3P46sxTl2SM7KmJzbQdPbkcNY1q5NQPS40UUSvQXgQE6Z28HSoFK1GKWmYyeH2XjVlwtVMYJqvlg7P9rRO4Zg4aG8S1bfRiAi5x5"
                         "fG9UWtjj3nYW5lF7mpE99nFBq8ECcsK1WSCchgtf6CJVOnNuNydHufl831Kq3nfdiR2qwVUStyP7E3huiLfULtPPnSTgi5xq0LjRvmsqAElECUgz0w7JKp8Ht3cWvw4rESWaE"
                         "0RSH4LPnSAck50zFoFJzZuQqeS1OEkwQPv3nXbXlwl4IVMkMmt1XouDYeK2nVX15ySP8ixA6TekxX4DRPCzKBFxuNlqtOBn6rR0WMMvCBpxM04bG4ghWUVamv8uCzi1TlFsn5"
                         "LyllDRgPlwE69Ehmwl8n9CjqC07RZb6l1dgmY8Rw2cyevjOJiyrAUSAXHMhZmxCHsZMI7TBUx8escecfw4N9pVDQIYab4Hsijb8FmSKBlcmbnK8Ba3TbAyn59CfoyyHCXjCcp"
                         "d7G3UrI2xE9trriGe8vquASyuwJ81o6vn9aGC7Q0MFBSKtzmSIFVtaOen2VDu6c9srqz8UAfQsfMrHgWplbsFCQugBHPOIZPmO9gKF0fK6hXkp2AadhNFwhF5pio7vhR0W1fZ"
                         "3Uyr28Vd5iTEXGXYLIJgY7qIek74YRa66muVvID9O2yBX6nP3NX4iwujjl8rnyqasbosywW3RDaxMhHZ32UBkM8wm8UW82WezonCH4EuA8ydtHgLT8EANoVHEXilNNQ2ByX0V"
                         "H5X3t6jKsSQq9fo1mliyVHoNyFIsntojgfs8t6DZ6XiFK8OO0nPD4YGHuQt7LrNlxMozbGKAOFxNwUexbeRZIxSjtFUPJO0uSB3Y6DomiyPvIET3BL8AyNktpszujemAGz6sL"
                         "qI9A1lcdEeAQCcgwScubStFOi7aTGLfVsmzruGVPdBViXTWgevG9LuH4F4qLMvy2kc27WPzpmsWpPU2nmCQAP6jycmQqL8LVjvc3Nz1pGG876z4955P5xUke6ARyosKCACfG8"
                         "vyrvAXEn0mbx4lEAL00byhoGR7SZOtPrRtRAG4Yw1f9CGI0HuJL2QWrOkdxXlPN5IQYrfz9c2Tqn3HuHo0O6ajbdO7LfQpxxKrZ4i3OqPzEfbE66NZqtmlBAMJaWHGC2I46UR"
                         "ojoI07GJDEt487ReUiYhXhYJR5Zwh6uIUgbU8HrcQRmAxvLHBZnVfnAW5aJcMSi8iE1NJGPkRKUhMQZAFlNaz8obCVl7RkTNkUxHgLn1DVgOJ8w1IFNJaUsRVEfQJX2Zpp6kd"
                         "osLhEDC5tyl2y3tpOO1VCAhuHmXZy2EnYtXH2Glc8rQAzvSK53lRTzpWLcheapJ7aKGub67rRI3Rf4BXL7aOAgJqAco3WQERDbrMWLSgFA6VwP2qd6sjInwu28kjVqbh9ucf8"
                         "sV0hEANMi2rH49CdQPgCRJyELhbBESs2FEY7DTHOj03MyrgXXbG7FvFrjHcYFJNgk8HEE7PVPUnAD0mfLBGoTnhwUEzVJCMlq7mOJJIfM3qG3iudA3E2u3oPw2YNTa5fbdrCA"
                         "R45RQZCjc4hcxaDVaBGbqJirXtorNqxJwoPWcEyhDYqtQgQBEc4yTwzPPZgQr0TEmljhmzaedyoFF9L0wRjGfLgffl21PumnsGZOOgCn0KV4xHRfe85Nj2XCd7YXQvt2O0J3O"
                         "SEQPVPmQyEeqo3lzqD2g5MvqSuOO1aOmrI9WvfSBIafioixgW6740uIoDNuOll25dXhAH2TcDbarrnUplXBCG5814ID0H0CqgQQzCqVnR5Oydudkfvs1iRPVcgLAiM5SjN8cQ"
                         "Te92Kvcehb70Soid97wopp37OeveImNep7DQtQEyZ9znQsYy71vUYNyx6bxlDmlzoay0ZMiafJvOumWjK0ZSZG9nGlGNo9biooFmLZR4LjZZWJ3pgJfSE6BuQdehpEtxe0vr9"
                         "DIXT8PqXyMd9t88XeZozNr3HQpaDbEUP28svWDa8g3YQbPAe32VGMwwIyOhbKE07ADufuO7F73skxa1xDvM3hebOYA9Xfy4RrTi3dyOnYJWqPrSm5YuFYs28mJbXjtaJrZAKG"
                         "i4CMlF36zmKSjamRKJxu2TQoETHYT8Zgkxc0yD4ppyGupOmDgTmBiI7Xl4mw40LRddZGl9MDxgO5PAAxNxo0XrR2CnWXwPd1K9Y7aUZVAud4t0FJS7wQ2EqsFk5Nw50KMu1jp"
                         "dDazJVd1EqxK8S4J6sf6w5MCWaEsw5IpshJV0sSoqBEBVNs25xnW1G3Ga50zMlawsRvP1t0WX8gkigCsRI5csB4tE7Ef1PfgNI7mtzFuCQpee8l5cPuQ0VlhixrqMRm8XKD0o"
                         "62spYzSIQUkVX8WSmk8G8gLMR4Yl6aj9wgXP8hxemdNbjZAXNFxRXeDzGybh0bkuGMGnySK1ECMSx2OXzaH3Ki45PMmREnPh0qlTbmwiQjArR4wqoPcj5slpja1vBIENhQvwm"
                         "LUPzAYRpEMAQ5ogaUjFyyzngudg811lwFrOJOcRHrpcIpyvO9pBWFIbD8hox2yFcJcVfvV9Y7nCRi8KjKElE7UHUeTLbfIhmymM8CtNEJyvNdf4Q9tKzuboAH7R46VBdpwfAC"
                         "EHiXjRvi6j6KBxP0Aarn28qVTjtsWnsWtDRqAB5zUZMmD4wiX96FRYpllkgJEfViIGyEZG3qF7m8rJBryKu8RvTPBAqpgOkj504ek2NbXthyP7iQQ7zYl0bxY5ly4oDaMjlGx"
                         "ujHXyCoeYSFuVxc12igeGlbCWkXL5PLkQhA7zahKLf9yxlqqqsxssZWS64zKM6R9uq58iaB5wotLCP9iPl89uvRIw1v4x98A9yD7dUc2796YkjEjRFIuHN38AOnv5txnbJ7Be"
                         "4n53Rjl1JP0ZNUWGzubWmgFUq7cbbnWaFovtZ7MwI25Lkh0zVa07ZQpMlFyaQExUG0y6mLI9bnjQUJmwBoMRX6CY8w51W3lTjsyxYkkbmMW7EjdYLaq56nfZ7yg9isSigIM6u"
                         "ecro9xsdefWbtaUm090b7NL29iF7CTPbm6AmikIGu0CTUNKlnlxOB7k7bm3sm7xohddxPES3ZWOyGIsOI2UIXAVIQnHwqHWN3Kx7g1NcAUqNzuLRWXqDO6W53Ld46M5MG0ICS"
                         "e7IGRqfaf4hdTkUXzQRkuwUSTP3KxrkduAmjGvkXKbliv6hWgrsUgDzcNKSnklseiqrKJ8zdPxvAZoJcugdbkYTibqt2vQnFTIU6UV83d1e6XGyDdqDUJVv9eh2PgZtSKnkjG"
                         "EtMtElDdz2oOmylxx7FMLP8ttorIGJ7tCpPbtS3Ulu5f0zocHYZvyZfKt5bC1vkaEmgqGFLrRbX46DVyzibEphY7e8crOyyfaacXGE1XWGy5FQq64b46rkVTGdic7KnEJjsiE"
                         "JK5seov4WbhJ6kli2LExIi58j7Y0vctvjxn07N09tA1iEcL1U9f8Tu3WzwvMLowWkVtzKAAfv5Wu5e0sVBnOkM0jmDHJbORobHg9DVY9rqLoX5f089lCfGiI4AjumVIbIm5y5"
                         "hhsuYlXHnAxGUPX3YbYXzEPKiDdyk59nkKU7bLbNmEedHtGbmteRkZ9OqsfZsgXBhKIROf6QhvdPPViprQbdWREXYrR9oLq1kXZd8Iw0whehPCv1XSDO4pCfbULQjpVQ1ConP"
                         "Z462ZPgLWFSr4MIVZDv8DtyzfMzTbYFcA6rN1ldRQzbDmDCl5ZB9KJt3X1aFKBuoGy63shEBfuNtuMKgMCorOCeCnbzhAMv0eZ69cqRFyo28ung5qiZvIOkO3Yfa9TtPzniBB"
                         "BmAFtzN35gbmG9NCDUlTdwgx6Un7UX89gyaFnrhYTKzyhZnxYXtPv2RfFZSpzV77AJdtoYzWmsCCD2Y1L97wP5j4yPcXwj9kL2aaDv3mRmy4fVEFmvfRDinlknxXVDPuJiie4"
                         "4atD1OFYldkOZo9e5h9rNbE1cb7vuUKzWUYKsbLYkHa1cKcgYcfJgszvyPGhlq4FklUJhFL1vJi15fmkfeHntvPKkqDao4dJUf7AKkRFUVrprQ5s5svLiW5JmIT0P2WbN7Z2h"
                         "jfKzWMFnpPT0cDAB7B8xK1y96Qm7WVvwRev2qsyKrGUdL6UnDFrX1PL42O1kzIPxIF6kqdDC8oydfWHcekunUC6OO01WGBcwcWzr1Xm9KwTXsltoZ7skVCCmZhnoibcPDro4T"
                         "d5vCVPGv0jiWJuvnCKOwZPazlOx045nPYFb8eAfPb8V7GhCKIWm73VoNlCwFJQ08EuF0B1Bqoyg6GJyVx30HPmPOj0TcaAxeu4ex593pFAHZWHwOBPIpCTvjS35PtYamxEwoB"
                         "OYMNk2hH92ScDuReVGFnqHndLlgZ6UIuQFv6MeFV1BqkGFzUcVIwnVa2PCJmDnVd5KT4kjCf4mGnDQ7gK2pqz878LR11Zv16l77Tb8GfpZFlPBUaSxLc61SW4JkXCkC0RPa8k"
                         "U3esX2Qsy0neKw8nQhQGTOXadFONPHcr0HwNxfP7QnkWJCxZHwqcjIbSOxPAmNzgjnMuZSwSmQUNnjDAoXTnQC67n69hDg76ypaEQKOLc0KQQ1Kzyhv4qR7u2cVGLy87ruHUt"
                         "JHpCFObWkrGtaLbNLCIkxJFlCS4J0ohdtgeLUJc1SfvtRb2swOOWT2g5YkQaXZafr0cFUWjo8w7BUXTyUYxrOGB7IFILMA90UDfhAldzW8FEetMUnaC9ljS6CEe9TRmGhhChX"
                         "aURqplBWqBOA03ftKgxuZUwD5MFqaZGFZX9rdGeWaclWSyOr4Op6Zs0edBpieS24gGu5i2RJXESfOOF9ZN0rsiWYlxVL2X3DHS9rRbd4JLDqq3zSCeMUqtV42l1om4LKsraIm"
                         "FFRY7LHq1Lw4KcUI2oEuBDZ5MF1Yy78meQ4egfGdC9reR604H61B7WQ1bOnFLdHveO6Lc88lXOOEMrRcYTXdelVWMjXbvmTBHcfZhpxnRUQDZ8rMoyGPY9EbZbZkFLDS63vp4"
                         "Zqu5GDS83cuTxUdaz8BUgRGZZu69TcSsT78QNhZGjbAf8yJ9akgbJlfiRhZgnuiuXr56WwTFqMSRfQnJyW3QbCkTRCrS2V0eiRu6rsXVgtczTVOOFbHajOTxj9YF5gFWESdk3"
                         "J53uHmpW1Hq0bWzWVjA0vSuXHpbvr29eIF0CntPME8Ybyg4ClT8CAG6jkTqNRcqDsy1uhKaZ4ujtQR90PEt33YLYIFGcpjw2kmai0NeB7fSjuzRpTIpu7E8OPTw3je2zAWxDt"
                         "FPSteoECKqi6J5iMfG6miyxm8RYTcfwnjNpAtviednj2nK2DboSKGAywbof8vsHFZXkb5g2snJmdhKBBMSQig6R34SdlMN4Gb3l3YJDsQWrfUiKnZlQIr8ybVSUrm7omTAEgG"
                         "TwRkngSggi4vdpnP61d7jVb7kgLk3VOwraY0YUuwtoQta0DWhWgWLaYJONcBDKzw0ncn0qOLIAG0XvBVPlE9ypGlBRssHvqcHMM5uajg4Mfk1AyqnoYukYteBTOYONPuojONB"
                         "3fhamgDBVQOJi3BCD0N7REInoTvH0k0sD4OssqbF1CMWVi5TOqBJr14hTXabTB2HLVAQpDvHsiYaiAP3C1YJrQkorrDwEHW35qIYQhDeFQcMP0mCQNdfTPQY5WRrRorCmLhkF"
                         "INB7UEynOGNr2JiNFyUG1hvrN2AtypMeGlwiRwnas7q3AIhi8ubBq0hVX1kJP6zebIZhNv1xIHtWc9aEzlCO88RwWWYpYbez7tjwIfrjwVqmAn95nhgPQSnybemorzAKMCbLl"
                         "hwlokezGoNBo2j7vfH21zS6RLdB87xb6VOULCX0cPuGmf1qC1dyYfpe44TsIapOTKRZJ4ibSz5mRTq9nAmsP2xfZAWyy4n9x6far5rumWVORYyfDn1OoTVckEuRgXAxC0ztgi"
                         "urgIBCTFarJT1L71Dgl0GmFR8KNwnQFZMPGZktPRwrK9LRu8USIM5dvyQSYRJHk69fS3JlkKlUL04A3CjmowQKGfaFygsUbakqgMxWuFNmw9ICa95FTz4sn4BOEtQjW1Vf7E5"
                         "5LTeZcjoGI21SX5qVioSoRJ09MHdOf0Z7B6ay1eT3KDPPyG0g7R8tvvX22f8AevoXz7ivaWL2bs5cVyPbr4hiPyK5llvQryVVDl0G7y2W0B5aC7AD86pde0dx4FfBOLfsrBmZ"
                         "pVjX2muefDY9M41jx8keg1qPvNZSEYfWcqRqko2qqmk9yxMXUaKr8yeZSvhTlMvrjEcC3pKLpGdqgmq8OUHtr0g21kmbF3isPtVqSpjUkkMoG4KJcDEidRqCXYOnLyJ930JNG"
                         "dG4Il77vVAQ0cKU9thAWGgdSLhc2SN0y95rLlxlbtoRtF1QsyqJ7Mz3BVWdBKxDbHbsf3ni6HTIfHZj32xCDK9RrF4YkK6y6c1w9aFFjOS86y1tSu6nIhZpyaQBCO0U3mOFGh"
                         "FvS1L0DzScJz7PiCNAzG3v2xePMksaK1nXcTK1janTVjE5LYnKdw9xTi6puih0LbeXtgFhqjJKn6godWEREFt5LaALTMteO0E0ZH8f1LO82cw0cu0UmbvE73GwTRrkpxnSGN9"
                         "7HpZ7EvYtqQuvUKBgFjoRZ9TImnQ22mgCTp5GIaA05FhEzAEXW0bggTPsZEnxAAXcVe7smuei8Qa6bjVFrEn16yQi1EP9WJEO1TnErlhNgxNUb1l3O0aSxQzhz7EKnzac3l4H"
                         "Dvu2gTCTix1JVKERZDQ2OE4CKkcfQ9TvTapjI8mRpJGXt2fmuoevs9mJzYjrS9wuD0CjVpmbDcJLjhgpPioHIAJjxnbj7w5nrNnKlffcAnCWwM9OI0mVcDVqXNMNEvOzxJATX"
                         "2yd2FerVLVQYN7NXXFG0UOeC4oN4uy50RgnxWMB7Lju6jQPlS3ZOpIrerY1e1iUmkgCaW9DoaXbNot7eZRsXjKlnKXlyg8qJ2I3X0sHSe9cXdgRNcs2hlkeezANsAGVef84E1"
                         "8YMHdYDNLa0qBofL6QvOeVmNsn8st4EIMa4xoFEFdQCIRTiWiBXWQInt4R5Jloj6c4VqOffNN7q2XRnegnVEdCJfLNCSSVOSvgtZS3Sq2zO5dsbk9ydCTQYDDBv8bIxxs2iay"
                         "L4qndpQcJNhgEF2opSb6CLEvxbfsCYhKOqSFiWHviBKqtM69HIDeytZ37iJZj26TUqebGi9CZlZyjbRtnwmOtUqXgtnMGjueCDw5leIhYgvP247A0FtqNcOmFZotQs3GcKh6v"
                         "pX2Vaw6GcrnDuTL1X5v0DNuRbFxdLbkTG4ufJfK2byNr8FZIUW8W2EtJ4hRUcPSIFLdoAY8wkUVC9DDahy6QqiPBXZXbgI9QMVwsPRVXC5tHvcVHB2J0FATgDCL7AhA3VSNZO"
                         "tQ8EFiNijaw0T3j8hWG0cPcDwShfvi3h8K9p98h0CspHni7uLS5LjFfcCl8JDY3lrf1gQqnJljIZWsrJQYOaUZ5T4OIR9M3vA98MHvf5b23lrFnwpgCXEthkvQ8RjCrixa8db"
                         "kFJvvZ8jYqHAB1M4KfDCdnc3gzuC8dsJnykpCsRsgsfuCeJvdQtdP6IPD3PL8DvNME8VbnCaGNvMprQ3dTNrm6Bjhxw84lQeCfdpGwMjQCwmedMhgAKpOuNctSP3Mqpqeu2md"
                         "c8PEuhtBUo24W60qhJaws7SfhZLeMFBN6pJcqSTMdKRCHHCGFCtG0qsYTrMS0HqgG9gSro28F7Vnc0TU0V6UCOtY4YwzrLM2ueHsKaqQ0wiY9rie3csDfT92afj6tYDqbDExt"
                         "YwZULb0vIJT5KXfbj9rRjimMAxLcrKJ4dhsQPKwR52S8nHSGqjRweuN6GJqqfjUSQ9F5BxZdh7bKslHH4kz5buV6aT91C58wYjsMfiQl1JD1N4J7G0NVs0w8i9v4Q65zn24bb"
                         "gFPRYnwGdAnYJKZz1uVlc2p6WTYDLsdf6taT6jNvzwqBjtcDb5C9UwIZbeGQo1ulgkqDXeFHZGrK4Z27oXhOxKhwNxim5klcAb5vAnwwOyr1Aa0fbLMLOPKtciY2632sMZNNP"
                         "byMG90VE7Y3FY0X7J4luX5OcrT4SO8541Cyzoxh3bt9eWeLkRSFmik4F41d3l0oJUNzPt0XbwE6OD2f85QYwl1WGwuJZv0E4cwgJ57hjGJJS1WHZ2wYXfoKKRaTxZrD8Ya2v5"
                         "RUGReQcpHfuyRe064IzhiDrtUpUx90dkAVmL62lhJXkos5higzlcZSZu0vYSm3nsLurq9HJgde2uWyqoNO14Zf4Z77EYKvPZkHPTdSNbZwx0OgOLiDyc4oPkv6ifVbapwudYK"
                         "QReciRIQec2lp9Bq3LwnHu6fw5UOnc77zUS50k6MZilKOjAwb2ogIt7Xn7VsKtMLKU7Yc7hObjzvMPqe7XfCGbBaMkPZDPBrLjY30IFmFW7Ebe3UoOtIAlPDMeGiqxOJY5LAJ"
                         "tPViDQOkP37xl4aKEzPLHiQVcbSzACp78fLhcv7TTXXa0XWDh5g3BGEFLpU2cAd9J65fOIDk7qXZ7SVmfLP44UiiAxYOXKXPslInPrpwgVuosfoffpp0QWFQHSPjOnycXfrbS"
                         "H3Iu1mXpIa5NNlizyLSCUH6qP4RTva4KwlXR0oaT6GACm8iZ1VggBU67BNREv1cgf7FDgwPiohi1Ev8ynLby6bIcUZqYwWeIZmOnXHkHjV3VhyizLnUxUAlMaT0hn29xRMc5P"
                         "QlQWOKCqXIQMIp3AWaFj33AwMPA88Zj361hQS9mpxFTuLxtLOHo7tfK0T1niTjdtlvTiqwaG1RozArN4YbBabYlcfEt6DVhyesnZo2X21Do0ZFaJuo7a8jkxzAbucjZxRwgo8"
                         "KjCnLLibpEkdcXb3Muoa114HMVMX0SKocua3fUB1pnGkzGbDKfwPqSnzpzypa2Bb9apNUa1N9Bk6867GwfI9lVZEAsEis7DTrsHyiUDATHnqcthumb8M6SC6NyZzUtRrq4hP5"
                         "kl3PXZCSsugj4EO94cZQxXbJa3MNlUAcmZKagWCGMUnJiloiwCiW8cZnAXwNydzKwaS5B1lT6upTQ8lGzlO1bLTbSAkfSdSBcDmj4rk4aUuHXYqPzqZi17LGYhTvcPwjWmkh7"
                         "RzdxhP2XCxRUW5DSswUV8IsCmgJE0mgIBLJ7kIbdmRoA9YLAYy0IV7yJAn6xg5242DQHjBlgAWbAxrp8iB7j3DeGHsg7kZBXQRq3FKxxJ8ImeKZU1eY2NAgZQksLycQkR7AXg"
                         "afisLK7MmJfwYFBVqVowsBwggFFCkyC8amE271QZaoQOuvLGiHMZKXZ93nhoep5CeZHWa32hftwK5SKL2E4TriUcNyKoOGIb5yszHXFisVP6tzLcqUcXI8VjuwGCLCNcHd5ct"
                         "OaGbdT47I2ReOBeTkg1hTeoT0SjmkUm6p1HLHqKennOxhhTA5naf5ulE7NCfQ2k90ahI0aKoalhMAhATkZ4pMHnCWMl0kPatsD3qa8tbkRr5mfCNTEYXK0OWHaOIgzhhledzY"
                         "9YzCCZiIfLHdMRDAkFb8ro9gdAOB6qNMrpBihIa3e0rHpcjVdaWV2QoiG6v7nylKfRrs61wSmNregTzae1MlV7WS9Ug4r9qUYs4Reh3iSyJoCWbv9QAuoKaRVmmmjWR99vFdi"
                         "rCtqpsU9TcizAMdl6EwjQmhDLmrAcJ3WwsQX5RkIJZN6XQ29Fi2K9riEOpnXIbUjJXAMU1V3a1CxvkrMxfiNxbrI2TrTHqctVde0bp6MRX6JEfmubdfGyBFd7NmJneerWxiQW"
                         "PKG8tZmhVEzbWhHeUT4gyEj3CNqT1VE0GUQt8lbvAP30qwEMJGAZcI8lYlARIkH2dJJzhz65WlJsaoh24ldPH27NeBHfyAsY17NW4gMgATGiqkAjucuqrVSqlP07yPVLPr03o"
                         "A5kAWQ3HpG37Mu8fRK9ozlJIzcfTx7CtiMrvCMKPeVtDrs9k4Xy87oIT7vna6NsPdTpb1XATxq67u1OOFRbERK7akc5Xer6hBJgt6iYpZXSfcoIn2q1cnk3EpZYDPr8qxeqIt"
                         "GxuMzLnUbayjAcucITR3erS9qGdF3Nva1GaeYQ0PIwHsSUehbVkQatFtGPtiWIjoMaTv1nFWpETDcGqeuXL6xyt6gXZYUPOEstCsZ6SsyB7FaXN28lSbxcDFMtYTrzAFuY7QK"
                         "2kllvoBCRMBpL94p1zvFSG2UsLT5cArKZuFyiSzSn8tqa8RoQWEChysy60ep5r7e3EP4Q6KKJIjniPsK1dOjzhR4sRrOVruRYi2LNC7NU5ptxYRoMglUKGTVVseDbZnBbBbPa"
                         "GoSaSHcq0uHvbTLRyVGocJgmjxIb7wz5zKJFJzrAbIx7SeMXE6mMAm0ecSqYnz1y134BzNHBMtkWXVRqutefRl0bksSCskHNnY2GzmAGQRuSXyxD0xGLzk13jT8aziSrNAHCq"
                         "o00FjI4d92Ao4l1lTM08VQmnZud64NpRnswlkt1wJT8CYcuY6RHsqoHrdHemP2MBqf8fk8SjFjSxhLBvpJelBuUsfeLLzECEQgF2jdJSER9TAUbv3m1GqUvc1Lzf310RKHET1"
                         "RFL4PBERLXRMP73Yf3D56XIuLg3uET5m4k4pvBzCBrdXiZHqIbix7vwogEhUuWXha9KtpJLgbLuyWeLTkzYElSsVJgHJX6QQsmbmm5tt3RS4aFJrIZxZTFua2XrDlKdy8kLhD"
                         "kpcclsFSfXcniQze1ahh6oepyFlMPBIPGoxwq8uMFB3XmLFfLopxGrSlWPmyfOz3EJIF5ceUBX4fTZHQimAgl5VWPP5Pmx7hqB8iiuHPKAfjPNOHW2VqlcSvEPPGpWHUTAgp7"
                         "QPt0KP4f93j4CWAB17ChhljidMnqAkMKyIT5NgN29wYJphRUJFNdY9YcQFwsgPnK9or5cZsMmRprIC5X5Tx4VCi1o4qeml4tgQB9a5GYIhkkM6Ne0IYHNI9wddmC5aYqxLEdA"
                         "iVcakHRnG0hky1D87OILqHz1rippTaCnHrrNlZl8JtbEHUnvtajhl9AttexNWCNDt2XkqZdCvHdFmVvLVgm85mmdqaMQoqvn1hPvk9DcuhCzCu2qkKXtMZcJZrX96tMqjkCB0"
                         "Q1KgPvoxFJ5Hc3AyFhyIiICpElhaVfDm0A6xMJ7D6KKrNqlVkzIz5nHzDpBXcwIROilNWlCC2VVfWXya6ZRT9pYF3svhTfkWUUByZLPTdcm9O15BiKANWBFNopwTuMXWHCOvh"
                         "j2bLmncUldCkr0MKb7AYBccyLme2Mdr4twFVSK6RdyNyqY7X1wMkU6eskrgMvqg2ZFtpHjhs3qyEXbDzLsA70PvILAjqP90BZAje9XtyF49WjnPt1FqIwl7SwuCVnILfI6MSl"
                         "ftT98mgfxHAogaT8KZbGVXEFy1XY38rKAhkW5O84JpCMVeoyVMwTCs4jgRCmzvIdTn7RMbRVPcHemD95TlzjvQbW9M9fIV6Ibvzroqu2XK5BWlnk9rYKjK1V2ugmD8hhb2Niy"
                         "aojdLdZsk9a38SubFOkxmZHyzcvsEMpruhGu2MLo17xKXSIiGGriUmPYZSBgZbXRlP7MCWi11BDzLBELX36o50iSFFoRWzqmqEU30vierSfu2Fx7xXKnNMTnHQJKFFYMSXLqx"
                         "LH3ClMe8nvCUaHGpmWTT1qccVxqfpfmpPPElJnfPplDA3D6CNPu0ATaVaQGmcyDGQo4EUi0FOjEJY0izOzrcmB7F9XEw5AW2DlZsXJd2uY2KSRkCqkNWt3WWnHvaKepqwoCdW"
                         "1un9iJaNISd99CUWfaNQrNESChYLq4biJDbBBNtgQ4cQQQtFjqMs89WXgKlvnUNFQVEr50FIXmYBVNqdRXtbAgapqAl7jbXvvEdWIxibFweWhwN1bSoCHYvfcT2yaZoDsVqdQ"
                         "h3CzZOHWqQbHkNK7qWxITe0dzWAaZCOlis5keEOwi9SFKxrEuGgrOhDVW0KpZ0SMkPj9BZEdhtnBNMSOtnkq8kmydIuXuisRBvA9ctVdIXMQRynwH5DOSRttdHZpBSOxCyqld"
                         "nLDXkLm1bAKvvZ9wNRBXuqoTSYgvo4QJnvmQEg97jbiDZhit8HhjVIePlZYdi7VlivA1UBPlijZsoRYnrp9JCTa2F4Wt9vBrL4QOU9jcvTMnPNWnWE3LqFNc24dkS3kgisuWO"
                         "Ph79E03kTpF1Dgp5XO4OQZGyCD8DjthKkIIC14hlprOFaiRzmXmQ1yyTiho1tzstLQtpf0dIqR1yoV0GdpvuDMhuf4UIRkWD2qeUc3wJg4vxR4txhnWiGNk0FoVUmEecIXrGV"
                         "p9nUnPLh938rHDd9nXY2wRJmVOGN7o8h87YP0H6iALKAQa4gQPC40u3csXKclmntIkjk4Sj0px06ar8LT555PdIxfBBCgtY4iifF50CcKk1rxv2As9rJArFLKRExe77QV2NVl"
                         "1XLVha2rAn1ILfnSptB66fDuCgonbCWexsL1MXVeK3A3Sy2KljGW45elE6X4UYRt1dW2w0xCvFIv6CDD4rf656hkXexesgD4BtI4uNlIRjWzGjqk6u2zzbClgpwqBfzDCa10o"
                         "mEEktwvIWHp3aGfNdrAfwfcUGFqvV5cTaUgl0FY5pNrcQkNrGXEBpWTeJ9BoS5fKKnZSqu3QaDRkBYmYyc4djVb3mNV62KQ9O70J8ExsSvRD6zaNrNhn7ONV9Y8nIMqt2k6kb"
                         "sUd0mBosZNKoLy4LG3x1ktEbmGYhsts7IB1aJzqX5o4u6WRnkUrXLpLHrKXWtWTynZ5bxuSyYFHd8BOoZ9Vsi6jNnMEbvUMOP9G2dk29nvX7uYBvz6wFhmEztOAj9klDp9pUd"
                         "NlbXyIgb8EH4bOI2UGQ32XSxoC7BK5mryEYyXaAm2shtaRvvG0zyxQs6KUOqv4j7r6x8asOjWJ6W7x9KKKriEppIbTBa6fVP1UztZy5fk1wwDQ4F3qjwFurZzRaZV6diVXGL5"
                         "fzhkmyrL8egLxpqNLlygtWw8fvxOlIzuxEPkA3LQumkXzfrYlUR47mqx46aqYd3JcwfpvB1g4tPGJQ1vcbZVI61ZcQPmfdxwQbnMbbnOnx6dSHBMNz8IS576PKMEMj0DozZ09"
                         "jcf6FatcaIC31ReReZSgNFme45qx44REkwlQgM6io29LEkL9UUuVXkmtV7OhBWUnixXKrF2RBU3xuba0iUhoZuCPCMxLbj8aFFL0EgeYiwEZWaB8eWa4iZJosdeowesKgRNwx"
                         "vVodvqJAr40kbeSD31oikgBJEwXkGsfyBJACrFPHuqdML8wBo1JXZIQ1DpGF2LrThfTDt01RLU9GiftUOfNq4b3uZ1jKwhl2YgOS0M5thAI4Ng4XO6BoXJgdrV6XthKX4qio3"
                         "F9uEWEWuOEusbIjg0IGRk3uw9nhLyB3a7eNbAcDwvivOWVV8bsyUBYWgFdDAPSUP07x75vavlLB04hRBiEpRPEcQDCkXXj0CE0C04DRWAcrLgNnO7WmsgzXDoQqsFQd54YJDp"
                         "My8dY1K4pgKeSHGwdhzrOpfho98A4t2wTh8YBqMjy8HAMFDGKL6bJZxasrUxmKUTg6GpGf6zSpvIgK5JUH130gq6mHprfvF4KMf9cbqZUtx1UawNxrCilE4CPDQrw7Qmv3UVk"
                         "MwtQPPROQxitCtCKDPofgz5Unu8FkY6oqr4exZ70QtymhvkLdbsW9NEVKX80cm5ijT2HivR5Uf1Va51jzts36wMb6GPZvMhaewjdlQAFFMfpP4A95VwWD7DndLOxm7Xc6553X"
                         "Q0zLc8kpKdxMRPLGzbeFWOcmJPe8TFsBe8GTtnpId0GWg2gYRsWYKSV2HwekG5yq7JnQnLLLZJIkZ8Y0tkNc954OENyzCmrua2BYN8f6RGwFG7vnaT0HYSPjPsTVaEgxtuEgH"
                         "EX3iGd4yalB5QRDU6UUNuDdIwLA1BcoiAIlv7S4J4yJ0FATD7n1dy5pwjREsTLeKX9uyc8zEoqWfpiIP3oijXBU6fWE7EyCChWyWB7vExOkHGMtQwgtDm2cwQDuZ1kMFwFJja"
                         "tbpexnBF3LQndE0qRGJ5BUNiw7thh7Fl6tRLonMEK2m8M9CiVWb3qK7VwhUL0DJvuqDuR83feqX3cL6t5McBhl3kGkCtc49rEy4aeDRDajlJIw7MUz3i3DyUdDNEvXGKqogYD"
                         "Z2Fj7bWLmOQDauRfIQyXJBS3yOqhMIcuDYwpDQdisTFmIz1rpG8d24t8IA6ApHciJrcQruZnxsBniMIuRWeEqJvQW1rfABYVQzZKUTk10bBQ2PUGcztPd8dg3xXq5VWtVPFrk"
                         "2NxF5knmmYJXgwbbpLktEycjkAkj9AOwjLXV7X5BEw62nuT67vp1Sw4QutGZQvJbg4dftJ6dAQdKxzJuOErAzaDs8NGkpfN5juw1JDxBldI9OSnozFOHYGCm09LWtHJnWAph5"
                         "cHRIF9sGpHFvNHFo1cGsmPpfbMKNGqyPx0sJZumFavilrDEwGPUDVIsS2GvTsXupmP6eBwwkVgSKKfZq29Xej5aW2Xy4fMDfXy7xP9FaR3rwzorCgsrfjrjtJbD2kUrcZrXLB"
                         "LpQE6zaMAQbO6rUoUACLPLXE9DiB4ZehUD0p41odtBFSUenYUmnLnHEHdEfcGQ1A98782mNNT1Sg0IixvHIZtAZv849OyNFbV5454p7NNRDD0vTmsPKENZqI3Q5De8dryPPlL"
                         "2ulLkiY93MYdaF66iJtJ6i4IGj8ljxe5OFuix1f6XAvuXS5Q9SISVGtwEplJtuUUzpHlRPb3bF1yzyJoX8FFN1W8JzzpoJ32K2MjRA0BjFToUXl0q7Qndt6pRMfR2hRiTfl6N"
                         "PHilK2OBWhMFQ1AfoR2b2SJ8th8GR6Oyvbf2dodgqjQToLoc0fwSZAnI9eWRi2Pi5f3ZjD2lUaNNZeUfOCIjOFt40RAzBZ83RFVfQb2mTMBZHnjnaSb082n5rXzyDSk5gSQCI"
                         "JqHrqUgFDCIaXeBUqcB5Y76AJsoWMMQPkuMeoecv";

    int fd = open("data.txt", O_RDWR | O_CREAT);
    if (fd < 0)
    {
        LE_ERROR("Failed to create file.");
        exit(1);
    }

    write(fd, largeString, sizeof(largeString));

    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        LE_ERROR("Failed to reposition the file offset.");
        close(fd);
        exit(1);
    }

    LE_ASSERT(le_avdata_PushStream("Long string", fd, PushCallbackHandler, (void*)5) == LE_OK);
}

// pushing single element use dot as delimiter for resource path
void PushSingleDotDelimiter()
{
    LE_ASSERT(le_avdata_CreateResource("asset.value", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt("asset.value", 6) == LE_OK);
    LE_ASSERT(le_avdata_Push("asset.value", PushCallbackHandler, (void *)6) == LE_OK);
}

// pushing multiple element on resource path with a dot delimiter
void PushMultiDotDelimiter()
{
    LE_ASSERT(le_avdata_CreateResource("asset.value1", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("asset.value2", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("asset.value3", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("asset.value4", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);

    LE_ASSERT(le_avdata_SetInt("asset.value1", 8) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat("asset.value2", 4.56) == LE_OK);
    LE_ASSERT(le_avdata_SetString("asset.value3", "dotDelimiter") == LE_OK);
    LE_ASSERT(le_avdata_SetBool("asset.value4", false) == LE_OK);
    LE_ASSERT(le_avdata_Push("asset", PushCallbackHandler, (void *)7) == LE_OK);
}


//--------------------------------------------------------------------------------------------------
/**
 * Receives notification from avdata about session state.
 */
//--------------------------------------------------------------------------------------------------
static void SessionHandler
(
    le_avdata_SessionState_t sessionState,
    void* contextPtr
)
{
    if (sessionState == LE_AVDATA_SESSION_STARTED)
    {
        LE_INFO("Airvantage session started.");

        LE_INFO("Running test case: %d", TestCase);
        switch (TestCase)
        {
            case 1:
                PushNonExistentAsset();
                break;
            case 2:
                PushNotValidAsset();
                break;
            case 3:
                PushSingle();
                break;
            case 4:
                PushMulti();
                break;
            case 5:
                PushData();
                break;
            case 6:
                PushSingleDotDelimiter();
                break;
            case 7:
                PushMultiDotDelimiter();
                break;
            default:
                break;
        }
    }
    else
    {
        LE_INFO("Airvantage session stopped.");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Component initializer.  Must return when done initializing.
 * Note: Assumes session is opened.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    le_avdata_AddSessionStateHandler(SessionHandler, NULL);

    if (le_arg_NumArgs() >= 1)
    {
        const char* arg1 = le_arg_GetArg(0);
        TestCase = atoi(arg1);
    }

    le_avdata_RequestSession();
}
