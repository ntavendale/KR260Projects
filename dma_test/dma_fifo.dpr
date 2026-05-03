{$MODE DELPHIUNICODE}
program dma_fifo;

uses
  SysUtils,
  DmaDevice in 'DmaDevice.pas';

var
  LDmaDevice : TDmaDevice;
begin
  LDmaDevice := TDmaDevice.Create;
  try
    LDmaDevice.WriteToDma($B0140001);
  finally
    LDmaDevice.Free;
  end;
end.
