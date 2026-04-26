{$MODE DELPHIUNICODE}
program dmareg;

uses
  SysUtils,
  AxiDma in 'AxiDma.pas';

const
  BASE_REGISTER = $A0000000;
var
  LAxiDma : TAxiDma;
  LRegContnets: LongWord;
begin
  LAxiDma := TAxiDma.Create(BASE_REGISTER);
  try
    WriteLn('DMA Register contts for MM2S & S2MM');
    WriteLn('===================================');
    WriteLn('MM2S Registers:');
    LRegContnets := LAxiDma.GetControl(cMM2S);
    WriteLn(Format('Dma Control           0x%.8x', [LRegContnets]));
    LRegContnets := LAxiDma.GetStatus(cMM2S);
    WriteLn(Format('Dma Status            0x%.8x', [LRegContnets]));
    LRegContnets := LAxiDma.GetSourceAddressLower;
    WriteLn(Format('Src Address (Lower)   0x%.8x', [LRegContnets]));
    LRegContnets := LAxiDma.GetSourceAddressUpper;
    WriteLn(Format('Src Address (Upper)   0x%.8x', [LRegContnets]));
    LRegContnets := LAxiDma.GetBufferlength(cMM2S);
    WriteLn(Format('Buffer Length         0x%.8x', [LRegContnets]));
    WriteLn(' ');
    WriteLn('S2MM Registers:');
    LRegContnets := LAxiDma.GetControl(cS2MM);
    WriteLn(Format('Dma Control           0x%.8x', [LRegContnets]));
    LRegContnets := LAxiDma.GetStatus(cS2MM);
    WriteLn(Format('Dma Status            0x%.8x', [LRegContnets]));
    LRegContnets := LAxiDma.GetDestAddressLower;
    WriteLn(Format('Dest Address (Lower)  0x%.8x', [LRegContnets]));
    LRegContnets := LAxiDma.GetDestAddressUpper;
    WriteLn(Format('Dest Address (Upper)  0x%.8x', [LRegContnets]));
    LRegContnets := LAxiDma.GetBufferlength(cS2MM);
    WriteLn(Format('Buffer Length         0x%.8x', [LRegContnets]));
  finally
    LAxiDma.Free;
  end;  
end.

