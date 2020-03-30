#ifndef EMU_PIC_H
# define EMU_PIC_H


typedef struct PIC {
    uint8_t icw1, icw3, icw4, mask, ins, pend, mask2, vector, ocw2, ocw3;
    int icw, read;
} PIC;


extern PIC	pic, pic2;
extern int	pic_intpending;


extern void	pic_set_shadow(int sh);
extern void	pic_init(void);
extern void	pic_init_pcjr(void);
extern void	pic2_init(void);
extern void	pic_reset(void);

extern void	picint(uint16_t num);
extern void	picintlevel(uint16_t num);
extern void	picintc(uint16_t num);
extern int	picinterrupt(void);
extern void	picclear(int num);
extern void	dumppic(void);


#endif	/*EMU_PIC_H*/
