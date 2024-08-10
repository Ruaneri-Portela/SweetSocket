public class WebMain {
    public boolean requestSuccessful;
    public byte[] body;


    public byte[] returnBody() {
        return body;
    }

    public boolean returnRequestSuccessful() {
        return requestSuccessful;
    }

    public WebMain(String request) {
        body = "<html><body><h1>404 Not Found</h1></body></html>".getBytes();
        requestSuccessful = true;
    }
}
