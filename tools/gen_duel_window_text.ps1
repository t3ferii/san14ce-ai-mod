# duel_manual relay window (local299, [diagnostics] duel_manual_test): writes src/duel_window_font.h, the fill and edge coverage of
# every text the mod-drawn command window shows (the rehearsal artifact's canvas: 800 Nanum Myeongjo with a 5 px black stroke, the row
# digits in Nanum Brush Script with a 6 px stroke). Fonts: the installed NanumMyeongjoExtraBold.ttf / NanumBrush.ttf (Sandoll / NAVER,
# SIL Open Font License 1.1). GDI+ path fill (anti-aliased) = fill, the path widened by the stroke width (round joins) and filled with
# it = edge; cropped to the edge with a pad, 4-bit levels. Placement per mask: lx = crop left from the pen, ty = crop top from the em
# middle (the canvas textBaseline "middle": baseline - (ascent - descent) / 2), adv = the typographic advance. Run again only when a
# text changes; the DLL colours the masks itself (native_duel_digits.inl window).
param([string]$Out)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
if(-not $Out){$Out=Join-Path $taskProject 'src\duel_window_font.h'}
function Find-TaskFont([string]$name) {   # the machine's font folder, else the user's (a per-user font install)
    foreach($dir in @((Join-Path $env:WINDIR 'Fonts'),(Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\Fonts'))) {
        $p=Join-Path $dir $name
        if(Test-Path -LiteralPath $p){return $p}
    }
    throw "font missing: $name"
}
$taskMyeongjo=Find-TaskFont 'NanumMyeongjoExtraBold.ttf'
$taskBrush=Find-TaskFont 'NanumBrush.ttf'
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.Runtime.InteropServices;
using System.Text;
public static class DuelText {
    static byte[] Render(GraphicsPath[] paths,int W,int H) {
        using(var bmp=new Bitmap(W,H,PixelFormat.Format32bppArgb)) {
            using(var g=Graphics.FromImage(bmp)) {
                g.Clear(Color.Black);g.SmoothingMode=SmoothingMode.AntiAlias;g.PixelOffsetMode=PixelOffsetMode.HighQuality;
                foreach(var p in paths)g.FillPath(Brushes.White,p);
            }
            var d=bmp.LockBits(new Rectangle(0,0,W,H),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
            var buf=new byte[W*H*4];Marshal.Copy(d.Scan0,buf,0,buf.Length);bmp.UnlockBits(d);
            var a=new byte[W*H];for(int i=0;i<W*H;++i)a[i]=buf[i*4+2];
            return a;
        }
    }
    static string B36(int n) {const string k="0123456789abcdefghijklmnopqrstuvwxyz";var s="";do{s=k[n%36]+s;n/=36;}while(n>0);return s;}
    static string Rle(byte[] a,int W,int x0,int y0,int w,int h) {
        var sb=new StringBuilder();
        var lv=new int[w*h];
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)lv[y*w+x]=(a[(y0+y)*W+x0+x]+8)/17;
        for(int i=0;i<lv.Length;) {
            int v=lv[i],j=i;
            while(j<lv.Length && lv[j]==v)++j;
            int run=j-i;
            if(v==15){sb.Append('f').Append(B36(run)).Append('.');}
            else if(v==0 && run>=3){sb.Append('z').Append(B36(run)).Append('.');}
            else for(int k=0;k<run;++k)sb.Append("0123456789abcde"[v]);
            i=j;
        }
        return sb.ToString();
    }
    // one mask: "{w,h,lx,ty,adv,"fill","edge"}"
    public static string Mask(string fontFile,float size,float stroke,string text,int pad) {
        var pfc=new PrivateFontCollection();pfc.AddFontFile(fontFile);
        var fam=pfc.Families[0];
        var style=fam.IsStyleAvailable(FontStyle.Regular)?FontStyle.Regular:FontStyle.Bold;
        float em=fam.GetEmHeight(style),asc=fam.GetCellAscent(style),dsc=fam.GetCellDescent(style),k=size/em;
        float middle=asc*k-(asc-dsc)*0.5f*k;
        const float OX=60f,OY=60f;
        var fmt=(StringFormat)StringFormat.GenericTypographic.Clone();fmt.FormatFlags|=StringFormatFlags.MeasureTrailingSpaces;
        float adv;
        using(var probe=new Bitmap(4,4))using(var g=Graphics.FromImage(probe))using(var font=new Font(fam,size,style,GraphicsUnit.Pixel))
            adv=g.MeasureString(text,font,PointF.Empty,fmt).Width;
        int W=(int)Math.Ceiling(OX*2+adv+size),H=(int)Math.Ceiling(OY*2+size*1.6f);
        var path=new GraphicsPath();path.AddString(text,fam,(int)style,size,new PointF(OX,OY),fmt);
        var edge=(GraphicsPath)path.Clone();
        using(var pen=new Pen(Color.White,stroke)){pen.LineJoin=LineJoin.Round;edge.Widen(pen);}
        var f=Render(new[]{path},W,H);
        var e=Render(new[]{path,edge},W,H);
        int x0=W,y0=H,x1=-1,y1=-1;
        for(int y=0;y<H;++y)for(int x=0;x<W;++x)if(e[y*W+x]>8){if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;}
        if(x1<0)throw new Exception("empty mask: "+text);
        x0=Math.Max(0,x0-pad);y0=Math.Max(0,y0-pad);x1=Math.Min(W-1,x1+pad);y1=Math.Min(H-1,y1+pad);
        int w=x1-x0+1,h=y1-y0+1;
        int lx=(int)Math.Round(x0-OX),ty=(int)Math.Round(y0-(OY+middle)),ad=(int)Math.Round(adv);
        return "{"+w+","+h+","+lx+","+ty+","+ad+",\""+Rle(f,W,x0,y0,w,h)+"\",\""+Rle(e,W,x0,y0,w,h)+"\"}";
    }
}
'@
# (enum name, font, px, stroke, text, pad) in the order the DLL indexes them
$taskM=$taskMyeongjo;$taskB=$taskBrush
$taskRows=New-Object System.Collections.ArrayList
for($d=0;$d -le 9;++$d){[void]$taskRows.Add(@("MHead$d",$taskM,24,5,"$d",1))}
[void]$taskRows.Add(@('MHeadTilde',$taskM,24,5,'~',1))
[void]$taskRows.Add(@('MHeadHap',$taskM,24,5,'합',1))
$taskStance=@('공격중시','방어중시','투지중시','일발중시')
for($i=0;$i -lt 4;++$i){[void]$taskRows.Add(@("MNow$i",$taskM,22,5,('지금 '+$taskStance[$i]),1))}
# local306: command 10 퇴각 (the 교대 place once no 교대 can be made)
$taskCmd=@('공격중시','방어중시','투지중시','일발중시','필살기','기합','견수','급소','무쌍','교대','퇴각')
for($i=0;$i -lt 11;++$i){[void]$taskRows.Add(@("MCmd$i",$taskM,25,5,$taskCmd[$i],1))}
for($i=0;$i -lt 4;++$i){[void]$taskRows.Add(@("MFill$i",$taskM,25,5,('('+$taskStance[$i]+')'),1))}
[void]$taskRows.Add(@('MCross',$taskM,18,5,'×',1))
[void]$taskRows.Add(@('MSpecial',$taskM,25,5,'필살',1))
[void]$taskRows.Add(@('MBack',$taskM,25,5,'돌아가기',1))
[void]$taskRows.Add(@('MNoMember',$taskM,14,5,'대기 무장 없음',1))
# local305 (user 「교체는 1회로 제한하자」): under a 교대 the side already used
[void]$taskRows.Add(@('MSwapUsed',$taskM,14,5,'교대 사용함',1))
[void]$taskRows.Add(@('MOk',$taskM,32,5,'확정',1))
[void]$taskRows.Add(@('MAuto',$taskM,26,5,'자동',1))
# local305: the hint of a hold without a list (the start, a join); local299-304 「목록의 합을 누르면 빠짐」
[void]$taskRows.Add(@('MHint',$taskM,17,5,'태세를 고르고 확정 (필살·교대는 아래 버튼)',1))
# local306 (user 「퇴각 … 삼11 코드로 확인해서」): under a 퇴각 short of 투지, the 퇴각 hold's hint, and the status line under the strip
# (the critical's charge, both guard timers) composed from pieces
[void]$taskRows.Add(@('MLack',$taskM,14,5,'투지 부족',1))
[void]$taskRows.Add(@('MRetHint',$taskM,17,5,'퇴각하면 이 일기토는 집니다 (확정으로 퇴각)',1))
[void]$taskRows.Add(@('MStCrit',$taskM,17,4,'크리티컬',1))
[void]$taskRows.Add(@('MStReady',$taskM,17,4,'가능',1))
[void]$taskRows.Add(@('MStOf6',$taskM,17,4,'/6',1))
[void]$taskRows.Add(@('MStGuard',$taskM,17,4,'견수',1))
[void]$taskRows.Add(@('MStFoe',$taskM,17,4,'적 견수',1))
[void]$taskRows.Add(@('MStHap',$taskM,17,4,'합',1))
for($d=0;$d -le 9;++$d){[void]$taskRows.Add(@("MSt$d",$taskM,17,4,"$d",1))}
for($d=1;$d -le 5;++$d){[void]$taskRows.Add(@("MRow$d",$taskB,44,6,"$d",18))}
# local301 (user 「만화책 스럽게 난입 !~ 이런 큰글자로 한단어로 딱 스티커?처럼」): the join sticker's word, big brush with a thick stroke
[void]$taskRows.Add(@('MJoin',$taskB,160,16,'난입!',4))
# local303 (user 「아직도 하얀 글씨 뭐 뜨는데 아무것도 안보여 난입처럼 스티커화」): the other round events and the end as stickers too
# (three syllables a little smaller so the stickers read about the same size); local306 the stances' criticals (San11 0x509480
# moves 5 / 6 / 7: 연속 공격 / 자동 견수 / 투지 상승, four syllables smaller again) and 퇴각; local312 연장 (the 합 cap's extension)
foreach($taskStk in @(@('MStkSwap',160,'교대!'),@('MStkSpecial',136,'필살기!'),@('MStkCrit',160,'급소!'),@('MStkMusou',160,'무쌍!'),
                      @('MStkKiai',160,'기합!'),@('MStkGuard',160,'견수!'),@('MStkWin',136,'판정승!'),@('MStkDraw',136,'무승부!'),
                      @('MStkCombo',116,'연속 공격!'),@('MStkAutoGuard',116,'자동 견수!'),@('MStkSpirit',116,'투지 상승!'),@('MStkRetreat',160,'퇴각!'),
                      @('MStkExtend',160,'연장!'))) {
    [void]$taskRows.Add(@($taskStk[0],$taskB,$taskStk[1],16,$taskStk[2],4))
}
# local313 (user 「연장은 조금 크기 작게해서 우하단에 계속 떠있게하자」, 「반듯한 네모 느낌으로 감싸줄수 잇어? 정돈된 느낌을 주는게 좋을거 같아」):
# the persistent extension badge's text in the window's face (an upright plate, not a sticker)
[void]$taskRows.Add(@('MBadgeExtend',$taskM,36,5,'연장 +5합',1))
$taskLines=New-Object System.Collections.Generic.List[string]
$taskLines.Add('#pragma once')
$taskLines.Add('// Relay duel command window text (local299, [diagnostics] duel_manual_test), generated by tools/gen_duel_window_text.ps1 (do not edit):')
$taskLines.Add('// fill and edge coverage (the fill dilated by the stroke) of every text the mod-drawn window shows, cropped, 4-bit levels. Nanum')
$taskLines.Add('// Myeongjo ExtraBold 800 and Nanum Brush Script (Sandoll / NAVER, SIL Open Font License 1.1). rle: a hex digit 0-e = one pixel at that')
$taskLines.Add('// level, "z<n>." / "f<n>." = n pixels (base 36) at level 0 / 15. lx: crop left from the pen, ty: crop top from the em middle, adv:')
$taskLines.Add('// the advance (pixels at the size the window draws them).')
$taskLines.Add('namespace duel_window_font {')
$taskLines.Add('struct Mask {short w,h,lx,ty,adv;const char *fill,*edge;};')
$taskLines.Add('enum MaskId:int {'+(($taskRows | ForEach-Object {$_[0]}) -join ',')+',MCount};')
$taskLines.Add('static const Mask kMasks[MCount]={')
foreach($r in $taskRows){$taskLines.Add('    '+[DuelText]::Mask($r[1],[float]$r[2],[float]$r[3],$r[4],[int]$r[5])+',   // '+$r[0])}
$taskLines.Add('};')
$taskLines.Add('}')
[IO.File]::WriteAllText($Out,($taskLines -join "`n")+"`n",(New-Object Text.UTF8Encoding($false)))
"wrote $Out ($($taskRows.Count) masks, $((Get-Item -LiteralPath $Out).Length) bytes)"
