sim.exe: sim.sv cpu.sv mem.sv alu.sv stack.sv signals.sv cdtimer.sv \
	signalizer.sv decoder.sv mcu.sv uart.sv uart_mux.sv adc.sv \
	simple_timer.sv spi.sv kbc.sv i2c.sv
	iverilog -g2012 -o $@ $^

stack_tb.exe: stack_tb.sv stack.sv
	iverilog -g2012 -o $@ $^

alu_tb.exe: alu_tb.sv alu.sv
	iverilog -g2012 -o $@ $^

decoder_tb.exe: decoder_tb.sv decoder.sv
	iverilog -g2012 -o $@ $^

signalizer_tb.exe: signalizer_tb.sv signalizer.sv
	iverilog -g2012 -o $@ $^

signals_tb.exe: signals_tb.sv signals.sv common.sv signalizer.sv decoder.sv
	iverilog -g2012 -o $@ $^

clkdiv_tb.exe: clkdiv_tb.sv clkdiv.sv
	iverilog -g2012 -o $@ $^

cdtimer_tb.exe: cdtimer_tb.sv cdtimer.sv
	iverilog -g2012 -o $@ $^

cpu_tb.exe: cpu_tb.sv cpu.sv alu.sv stack.sv signals.sv cdtimer.sv \
	signalizer.sv decoder.sv
	iverilog -g2012 -o $@ $^

uart_tb.exe: uart_tb.sv uart.sv simple_timer.sv
	iverilog -g2012 -o $@ $^

uart_mux_tb.exe: uart_mux_tb.sv uart.sv simple_timer.sv uart_mux.sv
	iverilog -g2012 -o $@ $^

spi_tb.exe: spi_tb.sv spi.sv simple_timer.sv
	iverilog -g2012 -o $@ $^

i2c_tb.exe: i2c_tb.sv i2c.sv simple_timer.sv
	iverilog -g2012 -o $@ $^

.PHONY: clean
clean:
	rm -f *.exe

.PHONY: test_modules
test_modules: stack_tb.exe alu_tb.exe decoder_tb.exe signalizer_tb.exe \
	signals_tb.exe clkdiv_tb.exe cdtimer_tb.exe cpu_tb.exe uart_tb.exe \
	uart_mux_tb.exe spi_tb.exe
	@./test_module.sh stack_tb.exe
	@./test_module.sh alu_tb.exe
	@./test_module.sh decoder_tb.exe
	@./test_module.sh signalizer_tb.exe
	@./test_module.sh signals_tb.exe
	@./test_module.sh clkdiv_tb.exe
	@./test_module.sh cdtimer_tb.exe
	@./test_module.sh cpu_tb.exe
	@./test_module.sh uart_tb.exe
	@./test_module.sh uart_mux_tb.exe
	@./test_module.sh spi_tb.exe
