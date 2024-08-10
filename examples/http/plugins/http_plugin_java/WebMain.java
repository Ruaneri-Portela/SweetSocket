public class WebMain {
    public boolean requestSuccessful = false;
    public byte[] body;
    public String contentType;
    public String headerAppend;
    public int statusCode;

    public WebMain(String request) {
        String path = request.split(" ")[1];
        String prefix = "/java";
        if (path.startsWith(prefix)) {
            body = "<html><body><h1>Hello, World From JAVA!</h1></body></html>".getBytes();
            contentType = "text/html";
            statusCode = 200;
            requestSuccessful = true;
        }
        return;
    }
}
