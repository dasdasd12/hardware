#ifndef V5F_PRODUCT_H4V1_H
#define V5F_PRODUCT_H4V1_H

#ifdef __cplusplus
extern "C" {
#endif

#define V5F_PRODUCT_H4V1_OK             0
#define V5F_PRODUCT_H4V1_ERR_RUNTIME   (-1)

/*
 * Blocking owner of LTDC Layer1 while the product UI is deactivated.
 * The first invocation validates and preloads the committed Flash asset;
 * later invocations replay the already verified SDRAM copy.
 */
int v5f_product_h4v1_run(void);

/* Stable literal identifying the most recent product-player failure. */
const char *v5f_product_h4v1_last_failure(void);

#ifdef __cplusplus
}
#endif

#endif /* V5F_PRODUCT_H4V1_H */
