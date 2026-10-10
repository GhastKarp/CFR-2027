clc;
clear;
close all;

SkidPad3 = table2array(readtable('Skid Pad 3.csv'));

time = SkidPad3(: , 1); % Select time values from xlsx
time = time / 1e6;      % Convert to seconds

num = numel(time); % Store number of elements in array data

% Get average time between samples
% samplingIntervals = zeros(num, 1);
% for i = 1 : (num-1)
%     samplingIntervals(i) = time(i+1) - time(i);
% end
% samplingFreqAvg = 1 / mean(samplingIntervals)

samplingFreqAvg = 1/0.005;

% Select sus travel values from xlsx
FRData = SkidPad3(: , 2) / 300 * 3.3; % 300 deg. per 3.3v
FLData = SkidPad3(: , 3) / 300 * 3.3; % 300 deg. per 3.3v
RRData = SkidPad3(: , 4) / 100 * 3.3; % 100 ** per 3.3v
RLData = SkidPad3(: , 5) / 100 * 3.3; % 100 ** per 3.3v

SteerData = (SkidPad3(: , 5) + 180) / 360 * 3.3; % 360 deg. per 3.3v

% Data to be sent through the low-pass filter
Data = [(time(2 : end) - time(2)) (FRData(2 : end))];

% Fourier Transforms
FRFFT = abs(fftshift(fft(FRData)));
FLFFT = abs(fftshift(fft(FLData)));
RRFFT = abs(fftshift(fft(RRData)));
RLFFT = abs(fftshift(fft(RLData)));
SteerFFT = abs(fftshift(fft(SteerData)));

% Normalize FFTs around 1
FRFFT = FRFFT ./ max(FRFFT);
FLFFT = FLFFT ./ max(FLFFT);
RRFFT = RRFFT ./ max(RRFFT);
RLFFT = RLFFT ./ max(RLFFT);
SteerFFT = SteerFFT ./ max(SteerFFT);

nyFreq = samplingFreqAvg / 2;
frequencies = linspace(-nyFreq, nyFreq, num); % Create array of frequency values


out = sim("Anti_Aliasing_Filter.slx");

plotMode = 1;

if plotMode == 0
    subplot(2, 6, 1)
    plot(time, FRData, 'g')
    title('FRData')
    ylim([min(FRData) max(FRData)])
    xlim([time(1) time(end)])
    
    subplot(2, 6, 2)
    plot(time, FLData, 'b')
    title('FLData')
    ylim([min(FLData) max(FLData)])
    xlim([time(1) time(end)])
    
    subplot(2, 6, 3)
    plot(time, RRData, 'r')
    title('RRData')
    ylim([min(RRData) max(RRData)])
    xlim([time(1) time(end)])
    
    subplot(2, 6, 4)
    plot(time, RLData, 'y')
    title('RLData')
    ylim([min(RLData) max(RLData)])
    xlim([time(1) time(end)])


    subplot(2, 6, 7)
    plot(frequencies, FRFFT, 'g')
    title('FRFFT')
    set(gca, 'YScale', 'log')
    
    subplot(2, 6, 8)
    plot(frequencies, FLFFT, 'b')
    title('FLFFT')
    set(gca, 'YScale', 'log')
    
    subplot(2, 6, 9)
    plot(frequencies, RRFFT, 'r')
    title('RRFFT')
    set(gca, 'YScale', 'log')
    
    subplot(2, 6, 10)
    plot(frequencies, RLFFT, 'y')
    title('RLFFT')
    set(gca, 'YScale', 'log')
    
    
    
    subplot(2, 6, [5,6,11,12])
    plot(frequencies, FRFFT, 'g')
    hold on
    plot(frequencies, FLFFT, 'b')
    plot(frequencies, RRFFT, 'r')
    plot(frequencies, RLFFT, 'y')
    set(gca, 'YScale', 'log')
    hold off
elseif plotMode == 1

    % Original data (time & frequency domains)
    subplot(2, 3, 1)
    plot(time(1 : end - 16), out.UnfilteredData(17 : numel(time)), 'g')
    title('Unfiltered Data')
    xlabel('time (s)')
    ylabel('voltage (V)')

    subplot(2, 3, 4)
    plot(frequencies, FRFFT, 'g')
    set(gca, 'YScale', 'log')
    title('Unfiltered FFT')
    xlabel('frequency (Hz)')
    ylabel('power')


    % Filtered data (time & frequency domains)
    subplot(2, 3, 2)
    plot(time(1 : end - 16), out.FilteredData(17 : numel(time)), 'y')
    title('Filtered Data')
    xlabel('time (s)')
    ylabel('voltage (V)')

    % Get normalized FFT of incoming data from Simulink
    FilteredFFT = abs(fftshift(fft(out.FilteredData)));
    FilteredFFT = FilteredFFT ./ max(FilteredFFT);

    % Get updated frequency based on number of incoming samples
    filteredFrequencyRange = linspace(-nyFreq, nyFreq, numel(FilteredFFT));

    subplot(2, 3, 5)
    plot(filteredFrequencyRange, FilteredFFT, 'y')
    set(gca, 'YScale', 'log') 
    title('Filtered FFT')
    xlabel('frequency (Hz)')
    ylabel('power')


    % Both compared directly
    subplot(2, 3, 3)
    plot(time(1 : end - 16), out.UnfilteredData(17 : numel(time)), 'g')
    hold on
    plot(time(1 : end - 16), out.FilteredData(17 : numel(time)), 'y')
    xlabel('time (s)')
    ylabel('voltage (V)')
    hold off

    subplot(2, 3, 6)
    plot(frequencies, FRFFT, 'g')
    set(gca, 'YScale', 'log')
    hold on
    plot(filteredFrequencyRange, FilteredFFT, 'y')
    xlabel('frequency (Hz)')
    ylabel('power')
    hold off
end
