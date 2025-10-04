# Makefile for master.lib
# Version 0.23
#
# Small版の作成: nmake small
# Large版:	nmake large
# SmallとLarge:	nmake c
# Compact版:	nmake compact
# Medium版:	nmake medium
# C lib全部:    namke lib
# (Pascal版:	nmake pas)
# C全部とPascal: nmake all
#
# 環境変数 DEBUGが定義されていると、デバッグ情報をつけてアセンブルします。
#
#
##対象make:
# NMAKE 1.20以降 - Microsoft-C/C++ 7.0以降に付属(6.0のものでも平気かも(^^;)
# Borland Make 3.6以降 - Borland C++ 3.0以降に付属(以前のはそのままじゃ駄目)
#	この場合は、'nmake'を 'make -N'に置き換えて読んで下さい。
#
##上記以外のmakeを使用する場合:
# 置換マクロ $(var:old=new) が使えない makeの場合、(Borland 3.6未満など)
# MTLIBMOD = の定義のところをエディタで書き換えてください。
#
# テンポラリファイル作成機能がある makeの場合,
#  Borland 3.6未満ならばこんな感じに変形してください。
#   例:   command << param1 param2	# <<の部分には作成されたファイル名が
#	inline..			# 入るので、対応する機能をいれる
#	<<
#    の場合、以下のようにする
#	  command &&!
#	inline..
#	! param1 param2
#
# テンポラリファイル作成機能がない makeの場合,うーうーどうにかがんばってね(^^;
#
#
# 恋塚昭彦
# 92/11/15
# 92/11/30 for nmake
# 92/12/13 near, farを無理矢理展開
# 93/ 2/12 medium,compact追加
# 93/ 3/20 super.lib合併
# 93/ 2/27 gc_poly.lib合併
# 93/ 5/14 refine...?


# Select C Compiler -----------
#  0 : Microsoft C/C++ 6.0～7.0A, Visual C++ 1.0J
#  1 : Borland C++ 2.0～4.5
#  2 : Turbo C/C++
!ifndef CTYPE
CTYPE = 0
!endif

# Select Assembler ------------
#  0 : OPTASM
#  1 : TASM
!ifndef ASMTYPE
ASMTYPE = 0
!endif

#------------------------------
# ディレクトリの定義
LARGEDIR = large
SMALLDIR = small
MEDIUMDIR = medium
COMPACTDIR = compact

#------------------------------
# ライブラリモジュールなどの定義
#
#MASTER_TPU = ../master.tpu
#MASTERV_TPU = ../masterv.tpu
#MTTPU = $(MASTER_TPU)
#MTTPUS = $(MASTERV_TPU) $(MASTER_TPU)

ASM_IND = asm.ind

MTVERSION = 023
MTLIBARC = ../mtlib$(MTVERSION).lzh
MTH	 = ../master.h ../masters.h ../masterl.h ../masterc.h ../masterm.h \
	   ../mgr.h ../mjoy.h
COREDOC  = ../readme.$(MTVERSION) ../master.txt ../master.his
MTLIBMAN = ..\master.man ..\gc_poly.man ..\mtuser.man ..\bgm.man ..\master.tag \
	   ..\pf_auto.txt
MTDOCSRC = master.xx
BGMMANSRC = bgm.xx
MTSRCARC = ..\mtsrc$(MTVERSION).lzh
MTMANARC = ..\mtman$(MTVERSION).lzh

MTLIBINC = func.inc sio.inc super.inc clip.inc bgm.inc vgc.inc pf.inc

MTLIBSRC = version.asm \
	   tx.asm       txboxfla.asm txclear.asm  txfillca.asm txisknj2.asm\
	   txlocate.asm txputc.asm   txputca.asm  txputns.asm  txputnsa.asm\
	   txputs.asm   txputsa.asm  txsetcsr.asm txesc.asm    txgetcps.asm\
	   txbackup.asm txputp.asm   txputpa.asm  txputnp.asm  txputnpa.asm\
	   txshow.asm   txhide.asm   txgetput.asm txsmooth.asm txroll.asm  \
	   txrlarea.asm txrlupc.asm  txrldwc.asm  txrlupca.asm txrldwca.asm\
	   txrllec.asm  txrlleca.asm txrlric.asm  txrlrica.asm\
	   txpset.asm   txpreset.asm txpseta.asm  txwdota.asm  txshowpg.asm\
	   txstart.asm  txframe.asm  txvputs.asm  txvputsa.asm\
	   txboxfca.asm\
	   vt.asm       vtbackup.asm vtboxfla.asm vtclear.asm  vtcolr98.asm\
	   vtesc.asm    vtframe.asm  vtgetcps.asm vtlocate.asm vtputc.asm  \
	   vtputca.asm  vtputs.asm   vtputsa.asm  vtrefrs.asm  vtrefrsa.asm\
	   vtrldwc.asm  vtrldwca.asm vtrlupc.asm  vtrlupca.asm vtstart.asm \
	   vtgetcur.asm vtsetcur.asm vtsyslin.asm vtputns.asm  vtputnsa.asm\
	   vtgetput.asm vtx.asm      vtxstart.asm\
	   emsalloc.asm emsexist.asm emsfree.asm  emsmove.asm  emssetnm.asm\
	   emsenabl.asm emswrite.asm emsread.asm  emssize.asm  emsrealc.asm\
	   emsspace.asm emsmaphp.asm emssavem.asm emsrestm.asm emsgetsg.asm\
	   emsfindn.asm emsdosre.asm emsdoswr.asm\
	   grp.asm      grp400li.asm grpclear.asm grphide.asm  grpshow.asm \
	   grp200li.asm grpstart.asm grpend.asm   grpxldot.asm grpshown.asm\
	   grprddot.asm grpbakup.asm grpxorbf.asm grpcppg.asm  grpscrol.asm\
	   grpbget.asm  grpenter.asm grpmove.asm  grppput8.asm grppget8.asm\
	   grpuput8.asm grpuget8.asm grpscrlu.asm rottbl.asm   grppptd8.asm\
	   wfont.asm    wfntentb.asm grpwapts.asm grpwaput.asm grpwfpts.asm\
	   grpwfput.asm grpwkpts.asm grpwkput.asm grpgjput.asm grpgputs.asm\
	   grpwapca.asm grpwapsa.asm grppilod.asm grppicom.asm grppildu.asm\
	   grppifre.asm magloadp.asm magfree.asm  grpextmd.asm grpulpt8.asm\
	   grp256.asm   grpwkptl.asm grpwkptr.asm\
	   pal.asm      palshow.asm  palinit.asm  palsh100.asm palset16.asm\
	   sdiexist.asm sdigetpa.asm sdisetpa.asm\
	   b30.asm      b30getmo.asm b30ver.asm   b30line.asm  b30setmo.asm\
	   b30limit.asm b30getpa.asm b30clock.asm b30lock.asm  b30vsync.asm\
	   rsl.asm     \
	   rspexist.asm rspfree.asm  rspgetpa.asm rspsetpa.asm resdata.asm \
	   fil.asm      filgets.asm  filread.asm  filskipu.asm filclose.asm\
	   filexist.asm filputc.asm  filropen.asm filwrite.asm filcreat.asm\
	   filgetc.asm  filbasnm.asm fildelet.asm filw.asm     filseek.asm \
	   fillread.asm fillwrit.asm filsize.asm  filtime.asm  filapend.asm\
	   filsettm.asm filsplit.asm filsplsl.asm\
	   doschdir.asm dosgdfre.asm dosgetcw.asm dosgetdr.asm dosnobrk.asm\
	   dosputch.asm doscputs.asm dosgetch.asm dossetdr.asm doskeycl.asm\
	   dosputs.asm  dosputc.asm  dosbreak.asm dosvect.asm  dosabswr.asm\
	   dosabsre.asm dosgetky.asm dosgetev.asm dosmkdir.asm dosrmdir.asm\
	   dosmaked.asm dosputp.asm  doscputp.asm dosgetk2.asm dosalloc.asm\
	   dosfree.asm  dos29fak.asm dossetdt.asm dosfind.asm  dosmove.asm \
	   dosc.asm     dosargv0.asm doscopy.asm  doscreat.asm doscpts2.asm\
	   dosputs2.asm dosfindm.asm dosmaxfr.asm\
	   vs.asm       vsync.asm    vsyncwai.asm vsyncent.asm\
	   vsyncfrq.asm vgavsync.asm vgavwait.asm\
	   tim.asm      timer.asm   \
	   strprntf.asm strptoc.asm  strctop.asm  strcomma.asm striskj2.asm\
	   sjis2jis.asm jis2sjis.asm\
	   perform.asm  perfstr.asm  perfat.asm   perf.asm    \
	   xms.asm      xmsexist.asm xmsalloc.asm xmsfree.asm  xmsmove.asm \
	   xmssize.asm  xmsmaxfr.asm xmsspace.asm xmsrealc.asm\
	   keybeep.asm  keytable.asm keystart.asm keysetla.asm beepfreq.asm\
	   keyshift.asm gkey.asm     keysense.asm keyreset.asm keypresd.asm\
	   keybios.asm  keyback.asm \
	   vbeepfrq.asm vkeywait.asm vkeyto98.asm\
	   font.asm     fontread.asm fontwrit.asm fontentg.asm fontentc.asm\
	   fontcg.asm   fontentk.asm\
	   fontat.asm   fontatin.asm fontatre.asm fontatcg.asm\
	   gjread.asm   gjwrite.asm  gjputc.asm   gjputca.asm  gjputs.asm  \
	   gjputsa.asm  gjbackup.asm gjentryb.asm gjputp.asm   gjputpa.asm \
	   gjputni.asm  gjputnia.asm\
	   mouse.asm    mouse1.asm   mouse2.asm   mousei.asm   mousev.asm  \
	   mousmove.asm mousmicy.asm mousex.asm   mousexi.asm \
	   memalloc.asm memlaloc.asm\
	   sio.asm      siostart.asm sioread.asm  siowrite.asm sioputc.asm \
	   siobit.asm   sioputs.asm  siosigna.asm sioputp.asm \
	   pcmconvt.asm pcmplay.asm \
	   tblhrev.asm \
	   cursor.asm   csrarrow.asm csrcross.asm csrhand.asm  csrok.asm   \
	   curpat2.asm \
	   grpaputc.asm grpaputs.asm bfntexts.asm bfnthdrr.asm bfntid.asm  \
	   bftchnge.asm bftentry.asm bfthdran.asm bftpalsk.asm grpaputp.asm\
	   bftpalst.asm blackin.asm  blackout.asm txcursor.asm bfnthead.asm\
	   fntclose.asm fontentb.asm fontopen.asm fontlink.asm\
	   grpfput.asm  grpfputs.asm grpkput.asm  grpklput.asm grpkputs.asm\
	   grpfputp.asm grpbputs.asm grpbputp.asm grpbputc.asm\
	   largebyt.asm largeput.asm overoll8.asm overput8.asm repair.asm  \
	   repout.asm   rgbload.asm  sliceput.asm smallpt8.asm super1p8.asm\
	   super1pl.asm superbft.asm supercan.asm supercgb.asm superch.asm \
	   superchg.asm superchr.asm superclp.asm superin.asm  superl18.asm\
	   superl1p.asm supermch.asm superol8.asm superoll.asm superout.asm\
	   superpa.asm  superpat.asm superpt8.asm superput.asm superrep.asm\
	   superzom.asm vircopy.asm  superptr.asm superfre.asm superat.asm \
	   superptt.asm supertt8.asm supertin.asm superrpt.asm superpvr.asm\
	   supervrr.asm superpak.asm superzpt.asm superz1p.asm superems.asm\
	   superbak.asm supercln.asm overbk16.asm overdot8.asm supervb1.asm\
	   supervib.asm superwav.asm superwv1.asm superzmv.asm superzv1.asm\
	   superhrv.asm superdup.asm superptv.asm supert8v.asm\
	   virrep.asm   virtual.asm  whitein.asm  whiteout.asm window.asm  \
	   wordmask.asm virfree.asm  viroput8.asm\
	   smemlget.asm smemwget.asm smemrel.asm  mem.asm      memheap.asm \
	   hmemlalc.asm memassig.asm memasdos.asm memunasi.asm hmemmaxf.asm\
	   hmemreal.asm\
	   dosread.asm  doswrite.asm dosseek.asm  dosfsize.asm dosdrive.asm\
	   dosgets.asm  dosgetve.asm dossetvf.asm dossetvn.asm\
	   clip.asm     gc_clip.asm  gc_trian.asm\
	   gc_zoid.asm  gc_hline.asm koizoid.asm  trapez_c.asm edges.asm   \
	   gc_line.asm  gc_setco.asm gc_boxfi.asm gc_pset.asm \
	   gc_fill.asm  gc_tile.asm  gc_polgc.asm gc_thicf.asm gc_tile1.asm\
	   egc.asm      gc_andor.asm sin8.asm     koizoidx.asm gc_clipn.asm\
	   gc_polcx.asm make_lin.asm solid.asm    cutline.asm  gdcline.asm \
	   gdcwait.asm  gdc.asm      iatan2.asm   atan8.asm    isqrt.asm   \
	   gc_vline.asm gc_plvcx.asm sin7.asm    \
	   gc_circf.asm gc_clipl.asm gc_rboxf.asm gc_triax.asm gc_byteb.asm\
	   gc_circl.asm gc_circx.asm gc_bmesh.asm\
	   gdccircl.asm gdcoutpw.asm gdcwaite.asm\
	   egcscrll.asm egcscrlr.asm egcsftd.asm  egcsftda.asm egcsftl.asm \
	   egcsftla.asm egcsftr.asm  egcsftra.asm egcsftu.asm  egcsftua.asm\
	   sindeg.asm   atandeg.asm  iatandeg.asm\
	   js.asm       soundio.asm  jsstart.asm  jssense.asm  jsend.asm   \
	   jsanalog.asm aj.asm       ajstart.asm  atrtcmod.asm jskey.asm   \
	   vgcstart.asm vgcclear.asm vgcrddot.asm vgcsetco.asm vgcpset.asm \
	   vgchline.asm vgcvline.asm vgcboxfi.asm vgcline.asm  vgccircf.asm\
	   vgccircl.asm vgccircx.asm vgcbyteb.asm vgcbmesh.asm vgczoid.asm \
	   vgczoidx.asm vgcpolgc.asm vgcpolcx.asm vgcthicl.asm vgc_zoid.asm\
	   vgcrboxf.asm vgctrian.asm vgcbputs.asm vgcbputc.asm vgcbytbp.asm\
	   vgckputc.asm vgckputs.asm vgcfput.asm  vgcfputs.asm vgcplvcx.asm\
	   vgcline2.asm\
	   vgastadr.asm vgasetli.asm vg4pput8.asm vg4bmove.asm vg4bputs.asm\
	   vg4bputc.asm vg4uget8.asm vg4wfpts.asm vg4bget.asm  vg4spptr.asm\
	   vg4spvrr.asm vg4spzv1.asm vg4spvib.asm vg4spwav.asm vg4spwv1.asm\
	   vg4spin.asm  vg4rpout.asm\
	   at98sc.asm   at98scrl.asm vg4pget8.asm vg4uput8.asm at98g400.asm\
	   dacinit.asm  dacshow.asm  dacblin.asm  dacblout.asm dacwhin.asm \
	   dacwhout.asm\
	   vg4spput.asm vg4sppt8.asm vg4oput8.asm vg4spzom.asm vg4spzpt.asm\
	   vg4spclp.asm vg4sp1pl.asm vg4sp1p8.asm vg4lgput.asm vg4sppvr.asm\
	   vg4sprol.asm vg4orol8.asm vg4vcopy.asm vg4vrep.asm  vg4slput.asm\
	   machine.asm  getmachi.asm getmac98.asm getmacat.asm getmacfm.asm\
	   getmacdb.asm\
	   getcpu.asm   cx486rw.asm \
	   getvideo.asm setvideo.asm getvstat.asm setvstat.asm\
	   b_b_org.asm  b_mget.asm   b_r_sdat.asm b_st_ply.asm b_timer.asm \
	   b_c_play.asm b_pinit.asm  b_r_stat.asm b_s_mode.asm b_tmhook.asm\
	   b_data.asm   b_play.asm   b_sound.asm  b_s_msic.asm b_w_play.asm\
	   b_e_snd.asm  b_repeat.asm b_sp_ply.asm b_s_temp.asm b_w_snd.asm \
	   b_inifin.asm b_r_data.asm b_sp_snd.asm\
	   rand.asm     random.asm  \
	   fepexist.asm\
	   bcloser.asm  bclosew.asm  bdopen.asm   bfill.asm    bflush.asm  \
	   bgetc.asm    bopenr.asm   bopenw.asm   bputc.asm    bputs.asm   \
	   bputw.asm    bread.asm    bseek.asm    bseek_.asm   bwrite.asm  \
	   pf.asm       pfclose.asm  pfgetc.asm   pfgetw.asm   pfint21.asm \
	   pfopen.asm   pfread.asm   pfrewind.asm pfseek.asm

# 次の行は、置換マクロが使えない makeを使用する場合は
# MTLIBMOD = の続きは上にある MTLIBSRC の定義をそのまま持って来て、
# その中の ".asm"を全部消して使ってください。
MTLIBMOD = $(MTLIBSRC:.asm=)

MTTPUOBJ = fontopnp.obj dosp.obj
MTTPUSRC = fontopnp.asm dosp.asm

#

!ifdef NEAR
TYPE = S
TYPEB = s
CMODELOPT = -DMASTER_NEAR
!else
TYPE = L
TYPEB = l
CMODELOPT = -DMASTER_FAR
!endif

.c.obj :
	$(CC) $(CFLAGS) $(CMODELOPT) -c $*.c
.cpp.obj :
	$(CC) $(CFLAGS) $(CMODELOPT) -c $*.cpp
.obj.exe :
	$(CC) $(CFLAGS) $*.obj master$(TYPEB).lib
.c.exe :
	$(CC) $(CFLAGS) $*.c master$(TYPEB).lib
.cpp.exe :
	$(CC) $(CFLAGS) $*.cpp master$(TYPEB).lib

# Pascal Compiler Settings --------
TPC = tpc

# C Compiler Settings -------------
!if $(CTYPE) == 0
CC = cl -nologo -W4		# MSC用。 Quick Cなら cl を qclにすれば…??
CFLAGS = -G1 -A$(TYPE)
ICFLAGS = -Ox -AL		# makeind作成用オプション
!endif
!if $(CTYPE) == 1
CC = bcc -w
CFLAGS = -1 -Z -m$(TYPEB)
ICFLAGS = -Ox -ml
!endif
!if $(CTYPE) == 2
CC = tcc -w
CFLAGS = -1 -Z -m$(TYPEB)
ICFLAGS = -O -Z -ml
!endif


# Assembler Settings --------------

AFLAGS = /mx /t
USEIND = 0

!if $(ASMTYPE) == 0
AS = optasm
ASEP = ,
USEIND = 1
!endif
!if $(ASMTYPE) == 1
AS = tasm
AFLAGS = $(AFLAGS) /q /w2 /jJUMPS
ASEP = ,
USEIND = 1
!endif
!if $(ASMTYPE) == 2
AS = masm
ASEP = ,
!endif
!if $(ASMTYPE) == 3
AS = ml -nologo -W3 -WX -c -Zm
AFLAGS = -Cx
ASEP = -Fo
!endif

# Debug information settings ------
!ifdef DEBUG
!if $(CTYPE) == 0
AFLAGS = $(AFLAGS) /Zi
CFLAGS = $(CFLAGS) -Zi
!else
!if $(ASMTYPE) == 0
AFLAGS = $(AFLAGS) /ZT
!else
AFLAGS = $(AFLAGS) /Zi
!endif
CFLAGS = $(CFLAGS) -v
!endif
!endif

!if $(USEIND) == 1
# 間接ファイルを使ったアセンブルを行う場合
SMALLOBJ = $(MTLIBSRC) makeind.exe
LARGEOBJ = $(MTLIBSRC) makeind.exe
COMPACTOBJ = $(MTLIBSRC) makeind.exe
MEDIUMOBJ = $(MTLIBSRC) makeind.exe

!else
# 間接ファイルを使ったアセンブルを行わない場合( NMAKE専用 )
# これはちゃんと動かないぜ
# 各要素の頭に ディレクトリがつくようにしたいんだけどね
#MTLIBOBJ = $(MTLIBSRC:.asm=.obj)
#SMALLOBJ = $(SMALLDIR)\HOGE small\$(MTLIBOBJ: = small\)
#LARGEOBJ = $(LARGEDIR)\HOGE large\$(MTLIBOBJ: = large\)
#COMPACTOBJ = $(COMPACTDIR)\HOGE compact\$(MTLIBOBJ: = compact\)
#MEDIUMOBJ = $(MEDIUMDIR)\HOGE medium\$(MTLIBOBJ: = medium\)

.asm{$(SMALLDIR)}.obj :
	$(AS) $(AFLAGS) /DNEARMODEL $< $(ASEP)$@;
.asm{$(LARGEDIR)}.obj :
	$(AS) $(AFLAGS) /DFARMODEL $< $(ASEP)$@;
.asm{$(COMPACTDIR)}.obj :
	$(AS) $(AFLAGS) /DCOMPACTMODEL $< $(ASEP)$@;
.asm{$(MEDIUMDIR)}.obj :
	$(AS) $(AFLAGS) /DMEDIUMMODEL $< $(ASEP)$@;

!endif

##################################################
DEF : SMALL	# デフォルトは small model版作成

ALL : SMALL MEDIUM COMPACT LARGE PAS

C : SMALL LARGE

# ---------------------------------------------------
# master?.lib作成
#
MASTERL_LIB = ..\masterl.lib
MASTERS_LIB = ..\masters.lib
MASTERM_LIB = ..\masterm.lib
MASTERC_LIB = ..\masterc.lib

FAR : $(MASTERL_LIB)
LARGE : $(MASTERL_LIB)
$(MASTERL_LIB) : $(LARGEOBJ)
	if exist $(MASTERL_LIB) del $(MASTERL_LIB)
!if $(USEIND) == 1
	if not exist $(LARGEDIR)\nul mkdir $(LARGEDIR)
	makeind << $(LARGEDIR)\ $(AFLAGS) /DFARMODEL > $(ASM_IND)
$(MTLIBSRC)
<<NOKEEP

	$(AS) @$(ASM_IND)
!endif
	cd $(LARGEDIR)
	lib @<<
..\$(MASTERL_LIB)
y
$(MTLIBMOD)
..\masterl.lst;
<<NOKEEP

	cd ..

#-------------------------------------
NEAR : $(MASTERS_LIB)
SMALL : $(MASTERS_LIB)
$(MASTERS_LIB) : $(SMALLOBJ)
	if exist $(MASTERS_LIB) del $(MASTERS_LIB)
!if $(USEIND) == 1
	if not exist $(SMALLDIR)\nul mkdir $(SMALLDIR)
	makeind << $(SMALLDIR)\ $(AFLAGS) /DNEARMODEL > $(ASM_IND)
$(MTLIBSRC)
<<NOKEEP

	$(AS) @$(ASM_IND)
!endif
	cd $(SMALLDIR)
	lib @<<
..\$(MASTERS_LIB)
y
$(MTLIBMOD)
..\masters.lst;
<<NOKEEP

	cd ..

#-------------------------------------
COMPACT : $(MASTERC_LIB)
$(MASTERC_LIB) : $(COMPACTOBJ)
	if exist $(MASTERC_LIB) del $(MASTERC_LIB)
!if $(USEIND) == 1
	if not exist $(COMPACTDIR)\nul mkdir $(COMPACTDIR)
	makeind << $(COMPACTDIR)\ $(AFLAGS) /DCOMPACTMODEL > $(ASM_IND)
$(MTLIBSRC)
<<NOKEEP

	$(AS) @$(ASM_IND)
!endif
	cd $(COMPACTDIR)
	lib @<<
..\$(MASTERC_LIB)
y
$(MTLIBMOD)
..\masterc.lst;
<<NOKEEP

	cd ..

#-------------------------------------
MEDIUM : $(MASTERM_LIB)
$(MASTERM_LIB) : $(MEDIUMOBJ)
	if exist $(MASTERM_LIB) del $(MASTERM_LIB)
!if $(USEIND) == 1
	if not exist $(MEDIUMDIR)\nul mkdir $(MEDIUMDIR)
	makeind << $(MEDIUMDIR)\ $(AFLAGS) /DMEDIUMMODEL > $(ASM_IND)
$(MTLIBSRC)
<<NOKEEP

	$(AS) @$(ASM_IND)
!endif
	cd $(MEDIUMDIR)
	lib @<<
..\$(MASTERM_LIB)
y
$(MTLIBMOD)
..\masterm.lst;
<<NOKEEP

	cd ..

#-------------------------------------
# master.tpu作成

#TPU : $(MTTPUS)
#PAS : $(MTTPUS)
#$(MASTER_TPU) : $(MASTERL_LIB) master.pas $(MTTPUOBJ)
#	$(TPC) master.pas -E.. -Olarge

#$(MASTERV_TPU) : $(MASTERL_LIB) masterv.pas
#	$(TPC) masterv.pas -E.. -Olarge

#fontopnp.obj : fontopnp.asm
#	$(AS) $(AFLAGS) /DFARMODEL $*;

#dosp.obj : dosp.asm
#	$(AS) $(AFLAGS) /DFARMODEL $*;
#
#

#-------------------------------------
# master.man作成
MAN : ..\master.man ..\bgm.man
..\master.man : $(MTDOCSRC)
	xtr -e -nz -o=d:master.tmp master.xx
	mv d:master.tmp ..\master.man
#	xtr -e -nz -o=..\master.man master.xx

..\bgm.man : bgm.xx
	xtr -e -nz -o=..\bgm.man bgm.xx

..\master.tag : ..\master.man
	cd ..
	grep -on ^C: master.man > master.tag
	cd src

master.dic : ..\master.man
	sed -f masredic.sed master.man > master.dic

# asm間接ファイル作成プログラム作成---
MAKEIND : makeind.exe
makeind.exe : makeind.c # 依存ファイルにlibを入れるとループしてしまう(^^;)
	$(CC) $(ICFLAGS) makeind.c masterl.lib
#-------------------------------------

# 配布パック作成 ##############################
#
PACK : $(MTLIBARC) $(MTSRCARC) $(MTMANARC)

MTLIBARCS = $(COREDOC) $(MTH) ..\masters.lib ..\masterl.ldf
MTTPUARCS = $(COREDOC) $(MTTPU) master.pas
MTMANARCS = $(COREDOC) $(MTLIBMAN) ..\bfnt16.doc ..\par.lzh \
			..\calib.lzh ..\magpi014.lzh \
			..\masredic.lzh ..\programs.lzh ..\gpdemo05.lzh \
			..\shogi120.lzh ..\lmb058a.lzh ..\taglst14.lzh
MTSRCARCS = source.txt makefile $(MTLIBINC) $(MTLIBSRC) $(MTTPUSRC) \
		makeind.exe makeind.c

# 配布ライブラリ(C)セット作成
#
$(MTLIBARC) : $(MTLIBARCS)
	lha u $(MTLIBARC) @<<
$(MTLIBARCS)
<<NOKEEP

MASTERL_LDF = ..\masterl.ldf
LIB : $(MASTERS_LIB) $(MASTERL_LIB) $(MASTERC_LIB) $(MASTERM_LIB)
LDF : $(MASTERL_LDF)
$(MASTERL_LDF) : $(MASTERS_LIB) $(MASTERL_LIB) $(MASTERC_LIB) $(MASTERM_LIB)
	if exist $(MASTERL_LDF) del $(MASTERL_LDF)
	ldf -r $(MASTERS_LIB) $(MASTERC_LIB) $(MASTERL_LDF)
	ldf -r $(MASTERC_LIB) $(MASTERL_LIB) $(MASTERL_LDF)
	ldf -r $(MASTERL_LIB) $(MASTERM_LIB) $(MASTERL_LDF)

# 配布ライブラリマニュアル+サンプルセット作成
#
$(MTMANARC) : $(MTMANARCS)
	lha u $(MTMANARC) @<<
$(MTMANARCS)
<<NOKEEP


# 配布ソースセット作成
#
$(MTSRCARC) : $(COREDOC) $(MTH) $(MTSRCARCS)
	lha u $(MTSRCARC) @<<
$(COREDOC) $(MTH)
<<NOKEEP

	if exist SRC.LZH del SRC.LZH
	lha a -z -t SRC @<<
$(MTSRCARCS)
<<NOKEEP

	lha m $(MTSRCARC) SRC.LZH


#-------------------------------------
# obj用ディレクトリ作成
#
!if $(USEIND) == 1
$(LARGEDIR)\HOGE : 
	if not exist $(LARGEDIR)\nul mkdir $(LARGEDIR)
	type nul > $(LARGEDIR)\hoge
$(SMALLDIR)\HOGE :
	if not exist $(SMALLDIR)\nul mkdir $(SMALLDIR)
	type nul > $(SMALLDIR)\hoge
$(COMPACTDIR)\HOGE :
	if not exist $(COMPACTDIR)\nul mkdir $(COMPACTDIR)
	type nul > $(COMPACTDIR)\hoge
$(MEDIUMDIR)\HOGE :
	if not exist $(MEDIUMDIR)\nul mkdir $(MEDIUMDIR)
	type nul > $(MEDIUMDIR)\hoge
!endif

#-------------------------------------
#
#
ARCS = ..\master.lzh ..\mtdoc.lzh ..\mtsrc.lzh
BACK : $(ARCS)

BACKUP : $(ARCS)
	cd ..
	lha f master
	cd src

MTS = ../masters.lib ../masterl.ldf $(MTTPUS) \
      ../master.h \
      ../masterc.h ../masterl.h ../masterm.h ../masters.h ../mgr.h ../mjoy.h \
      ../master.his ../history.ﾎﾝﾄ ../mtshare.c ../newer.c \
      ../lock.h ../lockobj.h ../graph.h ../mouse.h ../circle.cpp ../graph.cpp \
      ../graph98.cpp ../graphdsv.cpp ../mtclass.doc ../joytest.cpp \
      ../calib.c ../calib.exe

..\master.lzh : $(MTS)
	lha u ..\master @<<
$(MTS)
<<NOKEEP


..\mtsrc.lzh : makefile
	lha u ..\mtsrc makefile *.inc *.asm *.pas source.txt makeind.c makeind.exe

MTDOCS = ..\master.txt ..\master.man ..\bfnt16.doc ..\gc_poly.man \
	 ..\mtuser.man ..\bgm.man \
	 master.xx ..\taglst14.lzh ..\vers.bat regs.sed

..\mtdoc.lzh : $(MTDOCS)
	lha u ..\mtdoc @<<
$(MTDOCS)
<<NOKEEP
