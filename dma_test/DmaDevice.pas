{$MODE DELPHIUNICODE}
unit DmaDevice;

interface 

uses
  SysUtils, Classes, Unix, BaseUnix;

const
  DEVICE_NMAME = '/dev/dma_test_driver';

type
  TDmaDevice = class
  protected
    FDeviceName: String;
    FFileDesriptor: Integer;
    function OpenDevice: Boolean;
    procedure CloseDevice;
  public
    constructor Create(ADeviceName: String = DEVICE_NMAME);
    destructor Destroy; override;
    function WriteToDma(AValue: Longint): Integer;
  end;

implementation 

constructor TDmaDevice.Create(ADeviceName: String = DEVICE_NMAME);
begin
  FDeviceName := ADeviceName;
end;

destructor TDmaDevice.Destroy;
begin
  CloseDevice;
  inherited Destroy;
end;

function TDmaDevice.OpenDevice: Boolean;
begin
  Result := FALSE;
  FFileDesriptor := fpOpen(FDeviceName, O_RDWR or O_SYNC);
  if (-1 = FFileDesriptor) then
  begin
    WriteLn('fpOpen failed with error ', GetLastOSError);
    EXIT;
  end;
  Result := TRUE;
end;

procedure TDmaDevice.CloseDevice;
begin
  if (FFileDesriptor > 0) then
    fpClose(FFileDesriptor);
end;

function TDmaDevice.WriteToDma(AValue: Longint): Integer;
begin
  Result := 0;
  if not OpenDevice then
    EXIT;
  try
    Result := fpWrite(FFileDesriptor, AValue, SizeOf(AValue));
  finally
    CloseDevice;
  end;  
end;
  
begin
end.