{$MODE DELPHIUNICODE}
program dmareg;

uses
  SysUtils,
  AxiDma in 'AxiDma.pas';

const
  BASE_REGISTER = $A0000000;
var
  LAxiDma : TAxiDma;
  LReg: LongWord;
begin
  WriteLn('Create');
  LAxiDma := TAxiDma.Create(BASE_REGISTER);
  try
    WriteLn('Get Control');
    LReg := LAxiDma.GetControlResgister(cMM2S);
    WriteLn(Format('MM2S Dma Control Register contains 0x%.8x', [LReg]));
  finally
    LAxiDma.Free;
  end;  
end.

